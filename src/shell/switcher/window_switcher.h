#pragma once

#include "capture/toplevel_capture.h"
#include "shell/switcher/window_switcher_tile.h"
#include "wayland/wayland_seat.h"

#include <cstddef>
#include <deque>
#include <memory>
#include <optional>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include <vector>

class AsyncTextureCache;
class CompositorPlatform;
class ConfigService;
class IpcService;
class RenderContext;
class WaylandConnection;
struct wl_output;

struct WindowSwitcherPreview {
  ScreencopyImage image;
  std::uint64_t revision = 0;
};
// Fullscreen Alt+Tab style window switcher with a centered 5×5 grid.
class WindowSwitcher {
public:
  WindowSwitcher() = default;
  ~WindowSwitcher();

  void initialize(
      WaylandConnection& wayland, RenderContext* renderContext, CompositorPlatform& platform, ConfigService* config,
      AsyncTextureCache* asyncTextures
  );
  void registerIpc(IpcService& ipc);
  void onOutputChange();
  void onToplevelChange();
  void show(wl_output* output);

  [[nodiscard]] bool isActive() const noexcept { return m_active; }
  [[nodiscard]] bool onPointerEvent(const PointerEvent& event);
  [[nodiscard]] bool onKeyboardEvent(const KeyboardEvent& event);

private:
  struct Instance;

  void hide();
  void refreshWindows();
  void setSelectedIndex(std::size_t index);
  void cycleSelection(int delta);
  void navigateGrid(int colDelta, int rowDelta);
  void activateSelected();
  void closeWindowAt(std::size_t index);
  void queuePreviews(bool refreshCached);
  void prunePreviews();
  void startNextCapture();
  void requestSceneUpdate();
  [[nodiscard]] bool matchesTrigger(const KeyboardEvent& event) const noexcept;
  [[nodiscard]] bool isModifierRelease(const KeyboardEvent& event) const noexcept;
  void ensureSurface();
  void destroySurface();
  void prepareFrame(Instance& instance, bool needsUpdate, bool needsLayout);
  void buildScene(Instance& instance, std::uint32_t width, std::uint32_t height);
  void positionGrid(Instance& instance, float screenW, float screenH);
  void syncGridSelection();

  WaylandConnection* m_wayland = nullptr;
  RenderContext* m_renderContext = nullptr;
  CompositorPlatform* m_platform = nullptr;
  ConfigService* m_config = nullptr;
  AsyncTextureCache* m_asyncTextures = nullptr;

  Instance* m_instance = nullptr;
  std::vector<WindowSwitcherEntry> m_windows;
  std::size_t m_selectedIndex = 0;
  std::size_t m_gridColumns = 5;
  std::unique_ptr<ToplevelCapture> m_capture;
  std::deque<std::uintptr_t> m_captureQueue;
  std::unordered_set<std::uintptr_t> m_capturedThisSession;
  std::optional<std::uintptr_t> m_captureInFlight;
  // 跨切换会话保留缓存帧，打开时立即显示，并由后台采集替换为新帧。
  std::unordered_map<std::uintptr_t, WindowSwitcherPreview> m_previews;
  std::uint64_t m_nextPreviewRevision = 1;
  wl_output* m_output = nullptr;
  bool m_active = false;
};
