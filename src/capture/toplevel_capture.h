#pragma once

#include "capture/screencopy_capture.h"

#include <functional>
#include <memory>
#include <optional>
#include <string>

struct zwlr_foreign_toplevel_handle_v1;
class WaylandConnection;

struct ToplevelCapturePending;

// Per-window capture through hyprland-toplevel-export-v1. Mirrors
// ScreencopyCapture's frame lifecycle but targets a single foreign toplevel
// instead of an output, so the window switcher can show real window previews.
// Only compositors exposing the protocol (Hyprland) support it; callers must
// degrade to icon-only tiles otherwise.
class ToplevelCapture {
public:
  using CompletionCallback = std::function<void(std::optional<ScreencopyImage>, std::string error)>;

  explicit ToplevelCapture(WaylandConnection& wayland);
  ~ToplevelCapture();

  [[nodiscard]] bool available() const noexcept;
  [[nodiscard]] bool busy() const noexcept { return m_busy; }
  [[nodiscard]] WaylandConnection& wayland() noexcept { return m_wayland; }

  void capture(zwlr_foreign_toplevel_handle_v1* handle, bool overlayCursor, CompletionCallback onComplete);
  void cancelInFlight();

  void fail(std::string message);
  void finish(ScreencopyImage image);
  void destroyPending();

private:
  WaylandConnection& m_wayland;
  bool m_busy = false;
  CompletionCallback m_onComplete;
  std::unique_ptr<ToplevelCapturePending> m_pending;
};
