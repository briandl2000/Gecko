#pragma once
#include "gecko/platform/windows_interface.h"

#include <deque>
#include <unordered_map>

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
  void PumpEvents() noexcept override;
  bool PollEvent(WindowEvent& outEvent) noexcept override;
  Extent2D GetClientSize(WindowHandle window) const noexcept override;
  void SetTitle(WindowHandle window, const char* title) noexcept override;
  DpiInfo GetDpi(WindowHandle window) const noexcept override;
  NativeWindowHandle GetNativeWindowHandle(
      WindowHandle window) const noexcept override;

private:
  static u64 NowNsSafe() noexcept;

  u64 m_NextId {0};
  std::unordered_map<u64, WindowState> m_Windows;
  std::deque<WindowEvent> m_Events;
};

}  // namespace gecko::platform
