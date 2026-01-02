#pragma once

#include "gecko/core/core.h"

#include "gecko/platform/window.h"

namespace gecko::platform {

struct PlatformConfig {
  WindowBackendKind WindowBackend{WindowBackendKind::Auto};
};

class PlatformContext {
public:
  GECKO_API virtual ~PlatformContext() = default;

  [[nodiscard]]
  GECKO_API static Unique<PlatformContext> Create(const PlatformConfig &cfg);

  GECKO_API virtual bool CreateWindow(const WindowDesc &desc,
                                      WindowHandle &outWindow) noexcept = 0;
  GECKO_API virtual void DestroyWindow(WindowHandle window) noexcept = 0;

  GECKO_API virtual bool IsWindowAlive(WindowHandle window) const noexcept = 0;
  GECKO_API virtual bool RequestClose(WindowHandle window) noexcept = 0;

  GECKO_API virtual void PumpEvents() noexcept = 0;
  GECKO_API virtual bool PollEvent(WindowEvent &outEvent) noexcept = 0;

  GECKO_API virtual Extent2D
  GetClientSize(WindowHandle window) const noexcept = 0;
  GECKO_API virtual void SetTitle(WindowHandle window,
                                  const char *title) noexcept = 0;
  GECKO_API virtual DpiInfo GetDpi(WindowHandle window) const noexcept = 0;
  GECKO_API virtual NativeWindowHandle
  GetNativeWindowHandle(WindowHandle window) const noexcept = 0;

private:
};

} // namespace gecko::platform
