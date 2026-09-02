// 验证 Windows Alt+Tab 风格的不满行居中。布局会水平移动最后一行，因此命中测试、
// 单元格局部坐标与提示框锚点必须使用同一偏移，否则交互位置会与磁贴位置错开。

#include "render/core/renderer.h"
#include "render/scene/input_area.h"
#include "render/scene/node.h"
#include "ui/controls/virtual_grid_view.h"

#include <cstddef>
#include <linux/input-event-codes.h>
#include <memory>
#include <optional>
#include <print>
#include <string_view>
#include <vector>

namespace {

  int gFailures = 0;

  void expect(bool condition, std::string_view message) {
    if (!condition) {
      std::println(stderr, "virtual_grid_view_centering_test: {}", message);
      ++gFailures;
    }
  }

  class StubRenderer final : public Renderer {
  public:
    TextMetrics measureText(
        std::string_view, float fontSize, FontWeight, float, int, TextAlign, std::string_view, TextEllipsize, bool
    ) override {
      return TextMetrics{.bottom = fontSize};
    }

    TextMetrics measureFont(float fontSize, FontWeight) override { return TextMetrics{.bottom = fontSize}; }

    void measureTextCursorStops(
        std::string_view, float, const std::vector<std::size_t>&, std::vector<float>&, FontWeight
    ) override {}

    void measureTextCursorStopsWrapped(
        std::string_view, float, const std::vector<std::size_t>&, float, std::vector<TextCursorStop>&, FontWeight
    ) override {}

    TextMetrics measureGlyph(char32_t, float) override { return TextMetrics{}; }

    TextureManager& textureManager() override { std::abort(); }
    [[nodiscard]] float renderScale() const noexcept override { return 1.0F; }
  };

  // 记录按下与释放时命中的项目，避免测试依赖网格的私有布局状态。
  class RecordingAdapter final : public VirtualGridAdapter {
  public:
    explicit RecordingAdapter(std::size_t count) : m_count(count) {}

    [[nodiscard]] std::size_t itemCount() const override { return m_count; }
    [[nodiscard]] std::unique_ptr<Node> createTile() override { return std::make_unique<Node>(); }
    void bindTile(Node&, std::size_t, bool, bool) override {}

    bool onPointerPress(std::size_t index, float, float, float, float) override {
      presses.push_back(index);
      return true;
    }

    bool onPointerRelease(std::optional<std::size_t> index) override {
      releases.push_back(index);
      return false;
    }

    std::size_t m_count;
    std::vector<std::size_t> presses;
    std::vector<std::optional<std::size_t>> releases;
  };

  constexpr float kGridWidth = 500.0F;
  constexpr float kCellHeight = 40.0F;
  constexpr float kGap = 10.0F;
  constexpr std::size_t kColumns = 4;
  // 10 项、4 列，形成 4 / 4 / 2 三行，最后一行不满。
  constexpr std::size_t kItemCount = 10;

  struct Setup {
    VirtualGridView grid;
    RecordingAdapter adapter;
    StubRenderer renderer;
    InputArea* area = nullptr;

    explicit Setup(std::size_t itemCount, bool center) : adapter(itemCount) {
      grid.setAdapter(&adapter);
      grid.setColumns(kColumns);
      grid.setSquareCells(false);
      grid.setCellHeight(kCellHeight);
      grid.setColumnGap(kGap);
      grid.setRowGap(kGap);
      grid.setCenterIncompleteRows(center);
      grid.setSize(kGridWidth, 300.0F);
      grid.layout(renderer);
      area = grid.focusArea();
    }

    // 在指定位置按下并释放；释放事件会返回该位置命中的项目。
    [[nodiscard]] std::optional<std::size_t> hitAt(float x, float y) {
      adapter.presses.clear();
      adapter.releases.clear();
      area->dispatchPress(x, y, BTN_LEFT, true);
      area->dispatchPress(x, y, BTN_LEFT, false);
      return adapter.releases.empty() ? std::nullopt : adapter.releases.front();
    }
  };

} // namespace

int main() {
  // 500px 宽、4 列、10px 间距，对应 117.5px 单元格宽度。
  constexpr float kCellWidth = (kGridWidth - 3.0F * kGap) / 4.0F;
  constexpr float kLastRowItems = 2.0F;
  constexpr float kLastRowWidth = kLastRowItems * kCellWidth + (kLastRowItems - 1.0F) * kGap;
  constexpr float kExpectedOffset = (kGridWidth - kLastRowWidth) * 0.5F;
  constexpr float kLastRowY = 2.0F * (kCellHeight + kGap) + kCellHeight * 0.5F;

  {
    Setup centered(kItemCount, true);
    expect(centered.area != nullptr, "centered grid has no input area");

    // 完整行不产生偏移，命中逻辑保持不变。
    expect(
        centered.hitAt(kCellWidth * 0.5F, kCellHeight * 0.5F) == std::optional<std::size_t>{0},
        "row 0 col 0 hits item 0"
    );
    expect(
        centered.hitAt(kGridWidth - kCellWidth * 0.5F, kCellHeight * 0.5F) == std::optional<std::size_t>{3},
        "row 0 col 3 hits item 3"
    );

    // 不完整的最后一行从预期偏移位置开始。
    expect(
        centered.hitAt(kExpectedOffset + kCellWidth * 0.5F, kLastRowY) == std::optional<std::size_t>{8},
        "centered row 2 tile 0 hits item 8"
    );
    expect(
        centered.hitAt(kExpectedOffset + kCellWidth * 1.5F + kGap, kLastRowY) == std::optional<std::size_t>{9},
        "centered row 2 tile 1 hits item 9"
    );

    // 最后一行原先靠左的第一个槽位现在应为空白。
    expect(!centered.hitAt(kCellWidth * 0.5F, kLastRowY).has_value(), "left-aligned slot of centered row is empty");

    // 提示框锚点必须落在居中后的磁贴内，而不是旧的靠左位置。
    float anchorX = 0.0F;
    float anchorY = 0.0F;
    expect(centered.grid.absoluteAnchorForIndex(8, anchorX, anchorY), "anchor resolves for last-row item");
    expect(
        anchorX >= kExpectedOffset && anchorX <= kExpectedOffset + kCellWidth, "anchor X falls inside the centered tile"
    );
  }

  {
    // 默认关闭居中时，最后一行仍保持靠左。
    Setup plain(kItemCount, false);
    expect(plain.area != nullptr, "plain grid has no input area");
    expect(
        plain.hitAt(kCellWidth * 0.5F, kLastRowY) == std::optional<std::size_t>{8},
        "without centering row 2 tile 0 stays left-aligned"
    );
  }

  {
    // 即使启用居中，完整的最后一行也不能产生偏移。
    Setup full(8, true);
    expect(full.area != nullptr, "full grid has no input area");
    constexpr float kFullLastRowY = 1.0F * (kCellHeight + kGap) + kCellHeight * 0.5F;
    expect(
        full.hitAt(kCellWidth * 0.5F, kFullLastRowY) == std::optional<std::size_t>{4},
        "complete last row keeps left alignment"
    );
  }

  if (gFailures != 0) {
    std::println(stderr, "virtual_grid_view_centering_test: {} failure(s)", gFailures);
    return 1;
  }
  return 0;
}
