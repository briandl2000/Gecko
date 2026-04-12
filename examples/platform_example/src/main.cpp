#include <gecko/core/boot.h>
#include <gecko/core/scope.h>
#include <gecko/core/services.h>
#include <gecko/core/services/log.h>
#include <gecko/core/services/modules.h>
#include <gecko/core/utility/thread.h>
#include <gecko/core/version.h>
#include <gecko/platform/platform_context.h>
#include <gecko/platform/platform_module.h>
#include <gecko/runtime/console_log_sink.h>
#include <gecko/runtime/event_bus.h>
#include <gecko/runtime/file_log_sink.h>
#include <gecko/runtime/immediate_logger.h>
#include <gecko/runtime/module_registry.h>
#include <gecko/runtime/ring_profiler.h>
#include <gecko/runtime/runtime_module.h>
#include <gecko/runtime/thread_pool_job_system.h>
#include <gecko/runtime/trace_file_sink.h>
#include <gecko/runtime/tracking_allocator.h>

using namespace gecko;
using namespace gecko::platform;

namespace {

namespace app::platform_example::labels {
inline constexpr ::gecko::Label App =
    ::gecko::MakeLabel("app.platform_example");
inline constexpr ::gecko::Label Main =
    ::gecko::MakeLabel("app.platform_example.main");
}  // namespace app::platform_example::labels

class PlatformExampleAppModule final : public ::gecko::IModule
{
public:
  [[nodiscard]] ::gecko::Label RootLabel() const noexcept override
  {
    return app::platform_example::labels::App;
  }

  [[nodiscard]] bool Startup(
      ::gecko::IModuleRegistry& /*modules*/) noexcept override
  {
    return true;
  }

  void Shutdown(::gecko::IModuleRegistry& /*modules*/) noexcept override
  {}
};

PlatformExampleAppModule g_AppModule;

}  // namespace

int main()
{
  runtime::TrackingAllocator trackingAlloc;
  runtime::RingProfiler ringProfiler(1 << 16);  // 64K events
  runtime::ImmediateLogger immediateLogger;     // Immediate logging

  runtime::ModuleRegistry moduleRegistry;
  runtime::EventBus eventBus;

  // Create job system with 4 worker threads
  runtime::ThreadPoolJobSystem jobSystem;
  jobSystem.SetWorkerThreadCount(4);

  // Use GECKO_BOOT system for proper service installation and validation
  // Services are in dependency order: Allocator -> JobSystem -> Profiler ->
  // Logger
  GECKO_BOOT((Services {.Allocator = &trackingAlloc,
                        .JobSystem = &jobSystem,
                        .Profiler = &ringProfiler,
                        .Logger = &immediateLogger,
                        .Modules = &moduleRegistry,
                        .EventBus = &eventBus}));
  // Set up trace file sink for profiling data after services are available
  // Sink auto-unregisters when destroyed
  runtime::TraceFileSink traceSink("gecko_trace.json");

  if (!traceSink.IsOpen())
  {
    GECKO_WARN(app::platform_example::labels::Main,
               "Failed to open trace profiler sink\n");
  }
  else
  {
    if (auto* profiler = GetProfiler())
      traceSink.RegisterWith(profiler);
  }

  // Now configure logging sinks - they auto-unregister when destroyed
  runtime::ConsoleLogSink consoleSink;
  runtime::FileLogSink fileSink("log.txt");

  if (auto* logger = GetLogger())
  {
    fileSink.RegisterWith(logger);
    consoleSink.RegisterWith(logger);
    logger->SetLevel(
        LogLevel::Info);  // Filter out Trace and Debug messages initially
  }
  {
    GECKO_FUNC(app::platform_example::labels::Main);
    GECKO_INFO(app::platform_example::labels::Main, gecko::VersionFullString());

    // Register library modules after boot (now that logging is configured).
    (void)InstallModule(runtime::GetModule());
    (void)InstallModule(platform::GetModule());
    (void)InstallModule(g_AppModule);

    PlatformConfig cfg = {};
    cfg.Backend = DisplayBackendKind::Auto;

    PlatformContext ctx = PlatformContext(cfg);

    WindowDesc windowDesc;
    windowDesc.Title = "Gecko Platform Example";
    windowDesc.Size = {1280, 720};
    windowDesc.Visible = true;
    windowDesc.Resizable = false;
    windowDesc.Mode = WindowMode::Windowed;

    WindowHandle window;
    GECKO_INFO(app::platform_example::labels::Main,
               "Creating application window...");
    if (!ctx.Windows().CreateWindow(windowDesc, window))
    {
      GECKO_ERROR(app::platform_example::labels::Main,
                  "Failed to create window\n");

      return 1;
    }
    GECKO_INFO(app::platform_example::labels::Main,
               "Window created successfully");

    // ── Demonstrate new window APIs ────────────────────────────────
    {
      // DPI
      DpiInfo dpi = ctx.Windows().GetDpi(window);
      GECKO_INFO(app::platform_example::labels::Main,
                 "Window DPI: %u (scale %.2f)", dpi.Dpi,
                 static_cast<double>(dpi.Scale));

      // Client size
      Extent2D size = ctx.Windows().GetClientSize(window);
      GECKO_INFO(app::platform_example::labels::Main, "Client size: %ux%u",
                 size.Width, size.Height);

      // Position
      math::Int2 pos = ctx.Windows().GetPosition(window);
      GECKO_INFO(app::platform_example::labels::Main,
                 "Window position: (%d, %d)", pos.X, pos.Y);

      // Native handle
      NativeWindowHandle native = ctx.Windows().GetNativeWindowHandle(window);
      GECKO_INFO(app::platform_example::labels::Main,
                 "Native handle: backend=%u, handle=%p",
                 static_cast<unsigned>(native.Backend), native.Handle);

      // Decoration state
      GECKO_INFO(app::platform_example::labels::Main, "Decorated: %s",
                 ctx.Windows().IsDecorated(window) ? "true" : "false");

      // Window state
      GECKO_INFO(app::platform_example::labels::Main, "Window state: %u",
                 static_cast<unsigned>(ctx.Windows().GetWindowState(window)));
    }

    bool running = true;
    // Avoid hanging forever in headless/Null-backend runs.
    // Run up to ~10 seconds unless a close is requested.
    u32 frameCount = 0;

    // Subscribe to window close-requested events.
    auto closeSub = gecko::SubscribeEvent(
        events::WindowCloseRequested,
        [](void* user, const gecko::EventMeta&, gecko::EventView) {
          *static_cast<bool*>(user) = false;
        },
        &running);

    // Log window key events to demonstrate event handling
    auto keyPressedSub = gecko::SubscribeEvent(
        events::WindowKey,
        [](void* /*user*/, const gecko::EventMeta& /*meta*/,
           gecko::EventView view) {
          const auto* payload =
              reinterpret_cast<const events::WindowKeyPayload*>(view.Data());
          GECKO_INFO(app::platform_example::labels::Main,
                     "WindowKey: windowId=%llu, key=%u, down=%u",
                     static_cast<unsigned long long>(payload->Window.Id),
                     static_cast<unsigned>(payload->Key),
                     static_cast<unsigned>(payload->Down));
        },
        nullptr);

    auto focusSub = gecko::SubscribeEvent(
        events::WindowFocusChanged,
        [](void* /*user*/, const gecko::EventMeta& /*meta*/,
           gecko::EventView view) {
          const auto* payload =
              reinterpret_cast<const events::WindowFocusChangedPayload*>(
                  view.Data());
          GECKO_INFO(app::platform_example::labels::Main,
                     "WindowFocus: windowId=%llu, focused=%u",
                     static_cast<unsigned long long>(payload->Window.Id),
                     static_cast<unsigned>(payload->Focused));
        },
        nullptr);

    auto movedSub = gecko::SubscribeEvent(
        events::WindowMoved,
        [](void* /*user*/, const gecko::EventMeta& /*meta*/,
           gecko::EventView view) {
          const auto* payload =
              reinterpret_cast<const events::WindowMovedPayload*>(view.Data());
          GECKO_INFO(app::platform_example::labels::Main,
                     "WindowMoved: windowId=%llu, pos=(%d, %d)",
                     static_cast<unsigned long long>(payload->Window.Id),
                     payload->X, payload->Y);
        },
        nullptr);

    auto resizedSub = gecko::SubscribeEvent(
        events::WindowResized,
        [](void* /*user*/, const gecko::EventMeta& /*meta*/,
           gecko::EventView view) {
          const auto* payload =
              reinterpret_cast<const events::WindowResizedPayload*>(
                  view.Data());
          GECKO_INFO(app::platform_example::labels::Main,
                     "WindowResized: windowId=%llu, size=%ux%u",
                     static_cast<unsigned long long>(payload->Window.Id),
                     payload->Width, payload->Height);
        },
        nullptr);

    GECKO_INFO(app::platform_example::labels::Main, "Entering main loop...");
    while (running && ctx.Windows().IsWindowAlive(window) && frameCount < 600)
    {
      GECKO_SCOPE_NAMED(app::platform_example::labels::Main, "MainLoop");

      {
        GECKO_SCOPE_NAMED(app::platform_example::labels::Main, "PumpEvents");
        ctx.PumpEvents();
        (void)gecko::DispatchEvents();
      }

      GECKO_SLEEP_MS(16);
      ++frameCount;

      if (frameCount % 60 == 0)
      {
        GECKO_COUNTER(app::platform_example::labels::Main, "FrameCount",
                      frameCount);
      }
    }

    if (running)
    {
      // Headless/timeout fallback: request a clean shutdown.
      GECKO_INFO(app::platform_example::labels::Main,
                 "Timeout reached, requesting clean shutdown");
      ctx.Windows().RequestClose(window);
    }

    GECKO_INFO(app::platform_example::labels::Main, "Destroying window...");
    ctx.Windows().DestroyWindow(window);

    // Unregister sinks before shutting down services
  }
  consoleSink.Unregister();
  fileSink.Unregister();
  traceSink.Unregister();

  GECKO_SHUTDOWN();

  std::printf("Application exited successfully\n");
  return 0;
}
