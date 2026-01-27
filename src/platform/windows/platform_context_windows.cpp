#include "gecko/platform/platform_context.h"

#if defined(_WIN32)

#include "../private/labels.h"
#include "../window_backend.h"
#include "gecko/core/scope.h"
#include "gecko/core/services/log.h"

namespace gecko::platform {

namespace {
class WindowsPlatformContext final : public PlatformContext
{
public:
  explicit WindowsPlatformContext(Unique<IWindowBackend> backend) noexcept
      : m_Backend(std::move(backend))
  {}

  bool CreateWindow(const WindowDesc& desc,
                    WindowHandle& outWindow) noexcept override
  {
    return m_Backend->CreateWindow(desc, outWindow);
  }

  void DestroyWindow(WindowHandle window) noexcept override
  {
    m_Backend->DestroyWindow(window);
  }

  bool IsWindowAlive(WindowHandle window) const noexcept override
  {
    return m_Backend->IsWindowAlive(window);
  }

  bool RequestClose(WindowHandle window) noexcept override
  {
    return m_Backend->RequestClose(window);
  }

  void PumpEvents() noexcept override
  {
    m_Backend->PumpEvents();
  }

  bool PollEvent(WindowEvent& outEvent) noexcept override
  {
    return m_Backend->PollEvent(outEvent);
  }

  Extent2D GetClientSize(WindowHandle window) const noexcept override
  {
    return m_Backend->GetClientSize(window);
  }

  void SetTitle(WindowHandle window, const char* title) noexcept override
  {
    m_Backend->SetTitle(window, title);
  }

  DpiInfo GetDpi(WindowHandle window) const noexcept override
  {
    return m_Backend->GetDpi(window);
  }

  NativeWindowHandle GetNativeWindowHandle(
      WindowHandle window) const noexcept override
  {
    return m_Backend->GetNativeWindowHandle(window);
  }

private:
  Unique<IWindowBackend> m_Backend;
};
}  // namespace

Unique<PlatformContext> PlatformContext::Create(const PlatformConfig& cfg)
{
  GECKO_FUNC(labels::General);
  GECKO_INFO(labels::General, "PlatformContext::Create (Windows)\n");

  Unique<IWindowBackend> backend = CreateWindowBackend(cfg.WindowBackend);
  return CreateUnique<WindowsPlatformContext>(std::move(backend));
}

}  // namespace gecko::platform

#endif  // _WIN32
