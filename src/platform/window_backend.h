#pragma once

#include "gecko/platform/window.h"

namespace gecko::platform {

class IWindowBackend
{
public:
  virtual ~IWindowBackend() = default;

  virtual bool CreateWindow(const WindowDesc& desc,
                            WindowHandle& outWindow) noexcept = 0;
  virtual void DestroyWindow(WindowHandle window) noexcept = 0;

  virtual bool IsWindowAlive(WindowHandle window) const noexcept = 0;
  virtual bool RequestClose(WindowHandle window) noexcept = 0;

  virtual void PumpEvents() noexcept = 0;
  virtual bool PollEvent(WindowEvent& outEvent) noexcept = 0;

  virtual Extent2D GetClientSize(WindowHandle window) const noexcept = 0;
  virtual void SetTitle(WindowHandle window, const char* title) noexcept = 0;
  virtual DpiInfo GetDpi(WindowHandle window) const noexcept = 0;
  virtual NativeWindowHandle GetNativeWindowHandle(
      WindowHandle window) const noexcept = 0;
};

IWindowBackend& GetNullWindowBackend() noexcept;

// Selects and returns the backend to use.
// - Auto: prefers best available backend for this build
// - X11/Wayland: uses that backend if available, otherwise falls back to Null
IWindowBackend& ResolveWindowBackend(WindowBackendKind requested) noexcept;

}  // namespace gecko::platform
