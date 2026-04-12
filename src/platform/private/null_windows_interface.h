#pragma once

#include "gecko/core/services/events.h"
#include "gecko/platform/platform_events.h"
#include "gecko/platform/windows_interface.h"

#include <unordered_map>
#include <vector>

namespace gecko::platform {

struct NullWindowEntry
{
  WindowDesc Desc {};
  Extent2D ClientSize {};
  math::Int2 Position {0, 0};
  platform::WindowState State {platform::WindowState::Normal};
  CursorMode Cursor {CursorMode::Normal};
  bool Decorated {true};
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
  void SetClientSize(WindowHandle window, Extent2D size) noexcept override;
  void SetTitle(WindowHandle window, const char* title) noexcept override;
  const char* GetTitle(WindowHandle window) const noexcept override;
  void SetPosition(WindowHandle window, math::Int2 pos) noexcept override;
  math::Int2 GetPosition(WindowHandle window) const noexcept override;
  DpiInfo GetDpi(WindowHandle window) const noexcept override;
  NativeWindowHandle GetNativeWindowHandle(
      WindowHandle window) const noexcept override;

  void SetWindowState(WindowHandle window,
                      platform::WindowState state) noexcept override;
  platform::WindowState GetWindowState(
      WindowHandle window) const noexcept override;
  void SetDecorated(WindowHandle window, bool decorated) noexcept override;
  bool IsDecorated(WindowHandle window) const noexcept override;
  void RequestFocus(WindowHandle window) noexcept override;

  void SetCursorMode(WindowHandle window, CursorMode mode) noexcept override;
  CursorMode GetCursorMode(WindowHandle window) const noexcept override;

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
      events::WindowMovedPayload Moved;
      events::WindowStateChangedPayload StateChanged;
    } Data {};
    u32 PayloadSize {0};
  };

  static u64 NowNsSafe() noexcept;

  u64 m_NextId {0};
  std::unordered_map<u64, NullWindowEntry> m_Windows;
  std::vector<StagedEvent> m_Staged;
};

}  // namespace gecko::platform
