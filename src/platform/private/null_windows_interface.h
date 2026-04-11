#pragma once

#include "gecko/core/services/events.h"
#include "gecko/platform/platform_events.h"
#include "gecko/platform/windows_interface.h"

#include <unordered_map>
#include <vector>

namespace gecko::platform {

struct WindowState
{
  WindowDesc Desc {};
  Extent2D ClientSize {};
  bool Alive {true};
};

class NullWindowsBackend final : public IWindowsBackend
{
public:
  bool CreateWindow(const WindowDesc& desc,
                    WindowHandle& outWindow) noexcept override;
  void DestroyWindow(WindowHandle window) noexcept override;
  bool IsWindowAlive(WindowHandle window) const noexcept override;
  bool RequestClose(WindowHandle window) noexcept override;
  Extent2D GetClientSize(WindowHandle window) const noexcept override;
  void SetTitle(WindowHandle window, const char* title) noexcept override;
  DpiInfo GetDpi(WindowHandle window) const noexcept override;
  NativeWindowHandle GetNativeWindowHandle(
      WindowHandle window) const noexcept override;

  /// Drain staged events (from RequestClose / DestroyWindow) and enqueue
  /// them on the global event bus.  No OS interaction for the null backend.
  void PumpEvents(const gecko::EventEmitter& emitter) noexcept override;

private:
  // Events staged outside the pump (DestroyWindow, RequestClose) are held
  // here and flushed on the next PumpEvents() call.
  struct StagedEvent
  {
    gecko::EventCode Code {0};
    union
    {
      events::WindowClosedPayload Closed;
      events::WindowCloseRequestedPayload CloseRequested;
    } Data {};
    u32 PayloadSize {0};
  };

  static u64 NowNsSafe() noexcept;

  u64 m_NextId {0};
  std::unordered_map<u64, WindowState> m_Windows;
  std::vector<StagedEvent> m_Staged;
};

}  // namespace gecko::platform
