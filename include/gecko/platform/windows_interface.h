#pragma once
#include "gecko/core/api.h"
#include "gecko/core/ptr.h"
#include "gecko/platform/platform_config.h"
#include "gecko/platform/window.h"

namespace gecko::platform {

class IWindowsBackend
{
public:
  virtual ~IWindowsBackend() = default;

  [[nodiscard]]
  GECKO_API static Unique<IWindowsBackend> Create(
      const PlatformConfig& cfg) noexcept;

  GECKO_API virtual bool CreateWindow(const WindowDesc& desc,
                                      WindowHandle& outWindow) noexcept = 0;
  GECKO_API virtual void DestroyWindow(WindowHandle window) noexcept = 0;

  GECKO_API virtual bool IsWindowAlive(WindowHandle window) const noexcept = 0;
  GECKO_API virtual bool RequestClose(WindowHandle window) noexcept = 0;

  GECKO_API virtual void PumpEvents() noexcept = 0;
  GECKO_API virtual bool PollEvent(WindowEvent& outEvent) noexcept = 0;

  GECKO_API virtual Extent2D GetClientSize(
      WindowHandle window) const noexcept = 0;
  GECKO_API virtual void SetTitle(WindowHandle window,
                                  const char* title) noexcept = 0;
  GECKO_API virtual DpiInfo GetDpi(WindowHandle window) const noexcept = 0;
  GECKO_API virtual NativeWindowHandle GetNativeWindowHandle(
      WindowHandle window) const noexcept = 0;

private:
};

}  // namespace gecko::platform
