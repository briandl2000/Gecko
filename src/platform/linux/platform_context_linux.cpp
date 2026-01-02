#include "gecko/platform/platform_context.h"

#if defined(__linux__)

#include "../categories.h"
#include "../window_backend.h"
#include "gecko/core/log.h"
#include "gecko/core/profiler.h"

namespace gecko::platform {

namespace {
class LinuxPlatformContext final : public PlatformContext {
public:
  explicit LinuxPlatformContext(IWindowBackend &backend) noexcept
      : m_Backend(&backend) {}

  bool CreateWindow(const WindowDesc &desc,
                    WindowHandle &outWindow) noexcept override {
    return m_Backend->CreateWindow(desc, outWindow);
  }

  void DestroyWindow(WindowHandle window) noexcept override {
    m_Backend->DestroyWindow(window);
  }

  bool IsWindowAlive(WindowHandle window) const noexcept override {
    return m_Backend->IsWindowAlive(window);
  }

  bool RequestClose(WindowHandle window) noexcept override {
    return m_Backend->RequestClose(window);
  }

  void PumpEvents() noexcept override { m_Backend->PumpEvents(); }

  bool PollEvent(WindowEvent &outEvent) noexcept override {
    return m_Backend->PollEvent(outEvent);
  }

  Extent2D GetClientSize(WindowHandle window) const noexcept override {
    return m_Backend->GetClientSize(window);
  }

  void SetTitle(WindowHandle window, const char *title) noexcept override {
    m_Backend->SetTitle(window, title);
  }

  DpiInfo GetDpi(WindowHandle window) const noexcept override {
    return m_Backend->GetDpi(window);
  }

  NativeWindowHandle
  GetNativeWindowHandle(WindowHandle window) const noexcept override {
    return m_Backend->GetNativeWindowHandle(window);
  }

private:
  IWindowBackend *m_Backend{nullptr};
};
} // namespace

Unique<PlatformContext> PlatformContext::Create(const PlatformConfig &cfg) {
  GECKO_PROF_FUNC(categories::General);
  GECKO_INFO(categories::General, "PlatformContext::Create (Linux)\n");

  IWindowBackend &backend = ResolveWindowBackend(cfg.WindowBackend);
  return CreateUnique<LinuxPlatformContext>(backend);
}

} // namespace gecko::platform

#endif // __linux__
