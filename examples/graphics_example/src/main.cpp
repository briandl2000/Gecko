#include <gecko/core/boot.h>
#include <gecko/core/scope.h>
#include <gecko/core/services.h>
#include <gecko/core/services/events.h>
#include <gecko/core/services/log.h>
#include <gecko/core/services/modules.h>
#include <gecko/core/version.h>
#include <gecko/graphics/graphics_device.h>
#include <gecko/platform/platform_context.h>
#include <gecko/platform/platform_module.h>
#include <gecko/runtime/console_log_sink.h>
#include <gecko/runtime/event_bus.h>
#include <gecko/runtime/module_registry.h>
#include <gecko/runtime/ring_logger.h>
#include <gecko/runtime/ring_profiler.h>
#include <gecko/runtime/runtime_module.h>
#include <gecko/runtime/thread_pool_job_system.h>
#include <gecko/runtime/tracking_allocator.h>

using namespace gecko;
using namespace gecko::platform;
using namespace gecko::graphics;

namespace app::graphics_example::labels {
inline constexpr ::gecko::Label App =
    ::gecko::MakeLabel("app.graphics_example");
inline constexpr ::gecko::Label Main =
    ::gecko::MakeLabel("app.graphics_example.main");
}  // namespace app::graphics_example::labels

namespace {

class AppModule final : public ::gecko::IModule
{
public:
  [[nodiscard]] ::gecko::Label RootLabel() const noexcept override
  {
    return app::graphics_example::labels::App;
  }

  [[nodiscard]] bool Startup(
      ::gecko::IModuleRegistry& /*modules*/) noexcept override
  {
    return true;
  }

  void Shutdown(::gecko::IModuleRegistry& /*modules*/) noexcept override {}
};

AppModule g_AppModule;

}  // namespace

int main()
{
  runtime::TrackingAllocator trackingAlloc;
  runtime::RingProfiler      ringProfiler(1 << 16);
  runtime::RingLogger        ringLogger(1 << 16);
  runtime::ModuleRegistry    moduleRegistry;
  runtime::EventBus          eventBus;
  runtime::ThreadPoolJobSystem jobSystem;
  jobSystem.SetWorkerThreadCount(4);

  GECKO_BOOT((Services{.Allocator = &trackingAlloc,
                       .JobSystem  = &jobSystem,
                       .Profiler   = &ringProfiler,
                       .Logger     = &ringLogger,
                       .Modules    = &moduleRegistry,
                       .EventBus   = &eventBus}));

  runtime::ConsoleLogSink consoleSink;
  if (auto* logger = GetLogger())
  {
    consoleSink.RegisterWith(logger);
    logger->SetLevel(LogLevel::Info);
  }

  {
    GECKO_FUNC(app::graphics_example::labels::Main);
    GECKO_INFO(app::graphics_example::labels::Main, gecko::VersionFullString());

    (void)InstallModule(runtime::GetModule());
    (void)InstallModule(platform::GetModule());
    (void)InstallModule(g_AppModule);

    // ── Window ────────────────────────────────────────────────────
    PlatformContext ctx(PlatformConfig{});

    WindowDesc windowDesc;
    windowDesc.Title     = "Gecko Graphics Example";
    windowDesc.Size      = {1280, 720};
    windowDesc.Visible   = true;
    windowDesc.Resizable = true;
    windowDesc.Mode      = WindowMode::Windowed;

    WindowHandle window = ctx.Windows().CreateWindow(windowDesc);
    if (!window.IsValid())
    {
      GECKO_ERROR(app::graphics_example::labels::Main,
                  "Failed to create window");
      return 1;
    }
    GECKO_INFO(app::graphics_example::labels::Main, "Window created");

    // ── Graphics device ──────────────────────────────────────────
    auto device = CreateGraphicsDevice();
    GECKO_INFO(app::graphics_example::labels::Main, "Graphics device created");

    // ── Swapchain ────────────────────────────────────────────────
    NativeWindowHandle native   = ctx.Windows().GetNativeWindowHandle(window);
    Extent2D           clientSz = ctx.Windows().GetClientSize(window);

    SwapchainDesc scDesc;
    scDesc.Width       = clientSz.Width;
    scDesc.Height      = clientSz.Height;
    scDesc.NumBackBuffers = 2;

    Swapchain swapchain = device->CreateSwapchain(native, scDesc);
    if (!swapchain.IsValid())
    {
      GECKO_WARN(app::graphics_example::labels::Main,
                 "No concrete graphics backend; running without a real swapchain"
                 " (NullDevice presents are no-ops)");
    }
    else
    {
      GECKO_INFO(app::graphics_example::labels::Main,
                 "Swapchain created (%ux%u, %u back buffers)", scDesc.Width,
                 scDesc.Height, scDesc.NumBackBuffers);
    }

    // ── Event subscriptions ───────────────────────────────────────
    bool running = true;

    auto closeSub = SubscribeEvent(
        events::WindowCloseRequested,
        [](void* user, const EventMeta&, EventView view) {
          const auto* payload =
              reinterpret_cast<const events::WindowCloseRequestedPayload*>(
                  view.Data());
          (void)payload;
          *static_cast<bool*>(user) = false;
        },
        &running);

    struct ResizeState
    {
      GraphicsDevice* Device;
      Swapchain*      SC;
    };
    ResizeState resizeState{device.get(), &swapchain};

    auto resizeSub = SubscribeEvent(
        events::WindowResized,
        [](void* user, const EventMeta&, EventView view) {
          const auto* payload =
              reinterpret_cast<const events::WindowResizedPayload*>(
                  view.Data());
          (void)payload;
          auto* state = static_cast<ResizeState*>(user);
          // Backend queries current size from the native window handle
          // stored inside Swapchain.Data.
          state->Device->ResizeSwapchain(*state->SC);
        },
        &resizeState);

    auto keySub = SubscribeEvent(
        events::WindowKey,
        [](void* user, const EventMeta&, EventView view) {
          const auto* payload =
              reinterpret_cast<const events::WindowKeyPayload*>(view.Data());
          if (payload->Down && !payload->Repeat &&
              payload->Key == KeyCode::Escape)
          {
            *static_cast<bool*>(user) = false;
          }
        },
        &running);

    GECKO_INFO(app::graphics_example::labels::Main,
               "Entering frame loop — press Escape or close window to quit");

    // ── Frame loop ────────────────────────────────────────────────
    while (running)
    {
      ctx.PumpEvents();
      (void)DispatchEvents();

      // With NullDevice this is a no-op. With a real backend this would
      // submit command lists and flip the swapchain.
      device->Present(swapchain);
    }

    // ── Cleanup ───────────────────────────────────────────────────
    device->DestroySwapchain(swapchain);
    ctx.Windows().DestroyWindow(window);

    GECKO_INFO(app::graphics_example::labels::Main, "Shutdown complete");
  }

  GECKO_SHUTDOWN();
  ::std::printf("Graphics example finished.\n");
  return 0;
}
