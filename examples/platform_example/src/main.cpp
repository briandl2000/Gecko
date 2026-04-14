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
#include <vector>

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

    WindowHandle window = ctx.Windows().CreateWindow(windowDesc);
    GECKO_INFO(app::platform_example::labels::Main,
               "Creating application window...");
    if (!window.IsValid())
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

    // ── App state for event callbacks ───────────────────────────────
    struct AppState
    {
      PlatformContext* Ctx;
      WindowHandle MainWindow;
      std::vector<WindowHandle> Spawned;
      bool Running {true};
    };

    AppState appState;
    appState.Ctx = &ctx;
    appState.MainWindow = window;

    // ── Log keybindings ────────────────────────────────────────────
    GECKO_INFO(app::platform_example::labels::Main,
               "─────────────────────────────────────────");
    GECKO_INFO(app::platform_example::labels::Main, "  Window Keybindings:");
    GECKO_INFO(app::platform_example::labels::Main,
               "  [1] Open small window        (400x300)");
    GECKO_INFO(app::platform_example::labels::Main,
               "  [2] Open large window        (1600x900, resizable)");
    GECKO_INFO(app::platform_example::labels::Main,
               "  [3] Open borderless window   (800x600, no decorations)");
    GECKO_INFO(app::platform_example::labels::Main,
               "  [4] Open fullscreen window");
    GECKO_INFO(app::platform_example::labels::Main,
               "  [W] Close last spawned window");
    GECKO_INFO(app::platform_example::labels::Main, "  [Escape] Quit");
    GECKO_INFO(app::platform_example::labels::Main,
               "─────────────────────────────────────────");

    // Avoid hanging forever in headless/Null-backend runs.
    u32 frameCount = 0;

    // ── Close handler ──────────────────────────────────────────────
    // Main window close → quit.  Child window close → destroy it.
    auto closeSub = gecko::SubscribeEvent(
        events::WindowCloseRequested,
        [](void* user, const gecko::EventMeta&, gecko::EventView view) {
          auto* state = static_cast<AppState*>(user);
          const auto* payload =
              reinterpret_cast<const events::WindowCloseRequestedPayload*>(
                  view.Data());

          if (payload->Window == state->MainWindow)
          {
            state->Running = false;
            return;
          }

          // Child window closed — remove from spawned list
          auto& spawned = state->Spawned;
          for (auto it = spawned.begin(); it != spawned.end(); ++it)
          {
            if (*it == payload->Window)
            {
              GECKO_INFO(app::platform_example::labels::Main,
                         "Child window %llu closed",
                         static_cast<unsigned long long>(it->Id));
              state->Ctx->Windows().DestroyWindow(*it);
              spawned.erase(it);
              break;
            }
          }
        },
        &appState);

    // ── Key handler — spawn / close windows ────────────────────────
    auto keyPressedSub = gecko::SubscribeEvent(
        events::WindowKey,
        [](void* user, const gecko::EventMeta& /*meta*/,
           gecko::EventView view) {
          auto* state = static_cast<AppState*>(user);
          const auto* payload =
              reinterpret_cast<const events::WindowKeyPayload*>(view.Data());

          // Only act on initial key-down, not repeats
          if (!payload->Down || payload->Repeat)
            return;

          auto& windows = state->Ctx->Windows();
          WindowDesc desc;

          switch (payload->Key)
          {
          case KeyCode::D1: {
            desc.Title = "Gecko - Small";
            desc.Size = {400, 300};
            desc.Resizable = false;
            desc.Mode = WindowMode::Windowed;

            WindowHandle h = windows.CreateWindow(desc);
            if (h.IsValid())
            {
              state->Spawned.push_back(h);
              GECKO_INFO(app::platform_example::labels::Main,
                         "Opened small window (id=%llu, 400x300)",
                         static_cast<unsigned long long>(h.Id));
            }
            else
            {
              GECKO_ERROR(app::platform_example::labels::Main,
                          "Failed to open small window");
            }
            break;
          }

          case KeyCode::D2: {
            desc.Title = "Gecko - Large";
            desc.Size = {1600, 900};
            desc.Resizable = true;
            desc.Mode = WindowMode::Windowed;

            WindowHandle h = windows.CreateWindow(desc);
            if (h.IsValid())
            {
              state->Spawned.push_back(h);
              GECKO_INFO(app::platform_example::labels::Main,
                         "Opened large resizable window (id=%llu, 1600x900)",
                         static_cast<unsigned long long>(h.Id));
            }
            else
            {
              GECKO_ERROR(app::platform_example::labels::Main,
                          "Failed to open large window");
            }
            break;
          }

          case KeyCode::D3: {
            desc.Title = "Gecko - Borderless";
            desc.Size = {800, 600};
            desc.Decorated = false;
            desc.Mode = WindowMode::Windowed;

            WindowHandle h = windows.CreateWindow(desc);
            if (h.IsValid())
            {
              state->Spawned.push_back(h);
              GECKO_INFO(
                  app::platform_example::labels::Main,
                  "Opened borderless window (id=%llu, 800x600, no decor)",
                  static_cast<unsigned long long>(h.Id));
            }
            else
            {
              GECKO_ERROR(app::platform_example::labels::Main,
                          "Failed to open borderless window");
            }
            break;
          }

          case KeyCode::D4: {
            desc.Title = "Gecko - Fullscreen";
            desc.Mode = WindowMode::BorderlessFullscreen;

            WindowHandle h = windows.CreateWindow(desc);
            if (h.IsValid())
            {
              state->Spawned.push_back(h);
              GECKO_INFO(app::platform_example::labels::Main,
                         "Opened borderless-fullscreen window (id=%llu)",
                         static_cast<unsigned long long>(h.Id));
            }
            else
            {
              GECKO_ERROR(app::platform_example::labels::Main,
                          "Failed to open fullscreen window");
            }
            break;
          }

          case KeyCode::W: {
            if (state->Spawned.empty())
            {
              GECKO_INFO(app::platform_example::labels::Main,
                         "No spawned windows to close");
            }
            else
            {
              WindowHandle last = state->Spawned.back();
              GECKO_INFO(app::platform_example::labels::Main,
                         "Closing last spawned window (id=%llu)",
                         static_cast<unsigned long long>(last.Id));
              windows.DestroyWindow(last);
              state->Spawned.pop_back();
            }
            break;
          }

          case KeyCode::Escape:
            state->Running = false;
            break;

          default:
            break;
          }
        },
        &appState);

    // ── Other event logging ────────────────────────────────────────
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

    // ── Main loop ──────────────────────────────────────────────────
    GECKO_INFO(app::platform_example::labels::Main, "Entering main loop...");
    while (appState.Running && ctx.Windows().IsWindowAlive(window) &&
           frameCount < 600)
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

    if (appState.Running)
    {
      // Headless/timeout fallback: request a clean shutdown.
      GECKO_INFO(app::platform_example::labels::Main,
                 "Timeout reached, requesting clean shutdown");
      ctx.Windows().RequestClose(window);
    }

    // ── Cleanup — destroy all spawned windows, then main ───────────
    for (auto& h : appState.Spawned)
    {
      if (ctx.Windows().IsWindowAlive(h))
      {
        GECKO_INFO(app::platform_example::labels::Main,
                   "Destroying spawned window (id=%llu)",
                   static_cast<unsigned long long>(h.Id));
        ctx.Windows().DestroyWindow(h);
      }
    }
    appState.Spawned.clear();

    GECKO_INFO(app::platform_example::labels::Main,
               "Destroying main window...");
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
