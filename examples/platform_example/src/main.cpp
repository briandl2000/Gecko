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
#include <gecko/runtime/module_registry.h>
#include <gecko/runtime/ring_logger.h>
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
  SystemAllocator systemAlloc;
  runtime::TrackingAllocator trackingAlloc(&systemAlloc);
  runtime::RingProfiler ringProfiler(1 << 16);  // 64K events
  runtime::RingLogger ringLogger(1024);  // 1024 log entries in ring buffer

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
                        .Logger = &ringLogger,
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
    cfg.WindowBackend = WindowBackendKind::Auto;

    Unique<PlatformContext> ctx = PlatformContext::Create(cfg);

    WindowDesc windowDesc;
    windowDesc.Title = "Gecko Platform Example";
    windowDesc.Size = Extent2D {1280, 720};
    windowDesc.Visible = true;
    windowDesc.Resizable = false;
    windowDesc.Mode = WindowMode::Windowed;

    WindowHandle window;
    GECKO_INFO(app::platform_example::labels::Main,
               "Creating application window...");
    if (!ctx->CreateWindow(windowDesc, window))
    {
      GECKO_ERROR(app::platform_example::labels::Main,
                  "Failed to create window\n");

      return 1;
    }
    GECKO_INFO(app::platform_example::labels::Main,
               "Window created successfully");

    bool running = true;
    // Avoid hanging forever in headless/Null-backend runs.
    // Run up to ~10 seconds unless a close is requested.
    u32 frameCount = 0;
    GECKO_INFO(app::platform_example::labels::Main, "Entering main loop...");
    while (running && ctx->IsWindowAlive(window) && frameCount < 600)
    {
      GECKO_SCOPE_NAMED(app::platform_example::labels::Main, "MainLoop");

      {
        GECKO_SCOPE_NAMED(app::platform_example::labels::Main, "PumpEvents");
        ctx->PumpEvents();
      }

      {
        GECKO_SCOPE_NAMED(app::platform_example::labels::Main, "ProcessEvents");
        WindowEvent ev {};
        while (ctx->PollEvent(ev))
        {
          if (ev.Kind == WindowEventKind::CloseRequested)
          {
            GECKO_INFO(app::platform_example::labels::Main,
                       "Close requested, exiting main loop");
            running = false;
            break;
          }
        }
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
      ctx->RequestClose(window);
    }

    GECKO_INFO(app::platform_example::labels::Main, "Destroying window...");
    ctx->DestroyWindow(window);

    // Unregister sinks before shutting down services
  }
  consoleSink.Unregister();
  fileSink.Unregister();
  traceSink.Unregister();

  GECKO_SHUTDOWN();

  std::printf("Application exited successfully\n");
  return 0;
}
