#pragma once

#include "render/scene/input_area.h"
#include "ui/palette.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <string>

class AsyncTextureCache;
class Box;
class Flex;
class Glyph;
class Image;
struct ScreencopyImage;
class Label;
class Renderer;

struct WindowSwitcherEntry {
  std::string windowId;
  std::string title;
  std::string appId;
  std::string appLabel;
  std::string iconPath;
  std::uintptr_t closeHandle = 0;
};

// Windows 风格切换磁贴：顶部应用标题、完整窗口预览和双层焦点描边。
class WindowSwitcherTile : public InputArea {
public:
  WindowSwitcherTile(float contentScale, AsyncTextureCache* asyncTextures);

  [[nodiscard]] static bool
  hitTestCloseRegion(float cellWidth, float cellHeight, float contentScale, float localX, float localY) noexcept;

  void setCellSize(float cellWidth, float cellHeight);
  void setAppIconColorizeTint(std::optional<ColorSpec> tint) { m_appIconColorizeTint = tint; }
  void setOnInvalidate(std::function<void()> callback) { m_onInvalidate = std::move(callback); }
  void setCloseHovered(bool hovered);
  void bind(
      Renderer& renderer, const WindowSwitcherEntry& entry, bool selected, bool hovered, const ScreencopyImage* preview,
      std::uint64_t previewRevision
  );

private:
  void applyVisualState();
  void applyCloseVisualState();
  void applyPreview(Renderer& renderer, const ScreencopyImage* preview, std::uint64_t previewRevision);
  bool refreshIcon(Renderer& renderer);
  void layoutOverlays(Renderer& renderer);

protected:
  void doLayout(Renderer& renderer) override;

  float m_contentScale = 1.0F;
  float m_cellWidth = 0.0F;
  float m_cellHeight = 0.0F;
  float m_iconHostWidth = 0.0F;
  float m_iconHostHeight = 0.0F;

  Flex* m_layout = nullptr;
  Box* m_frame = nullptr;
  Box* m_focusShadow = nullptr;
  Box* m_focusRing = nullptr;
  Flex* m_inner = nullptr;
  Box* m_iconHost = nullptr;
  Flex* m_caption = nullptr;
  Box* m_closeBackdrop = nullptr;
  Glyph* m_closeGlyph = nullptr;
  Image* m_preview = nullptr;
  Image* m_headerIcon = nullptr;
  Image* m_icon = nullptr;
  Glyph* m_fallbackGlyph = nullptr;
  Label* m_title = nullptr;

  WindowSwitcherEntry m_entry;
  bool m_hasEntry = false;
  bool m_selected = false;
  bool m_hovered = false;
  bool m_closeHovered = false;
  std::string m_iconPath;
  std::uint64_t m_previewRevision = 0;
  int m_iconTargetSize = 0;
  int m_headerIconTargetSize = 0;
  AsyncTextureCache* m_asyncTextures = nullptr;
  std::optional<ColorSpec> m_appIconColorizeTint;
  std::function<void()> m_onInvalidate;
};
