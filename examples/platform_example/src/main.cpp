#include <gecko/core/engine.h>
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

void PrintHelp() noexcept
{
  GECKO_INFO(app::platform_example::labels::Main,
             "═════════════════════════════════════════════════════");
  GECKO_INFO(app::platform_example::labels::Main,
             "  Gecko Platform Example — Window API Showcase");
  GECKO_INFO(app::platform_example::labels::Main,
             "═════════════════════════════════════════════════════");
  GECKO_INFO(app::platform_example::labels::Main, "  Window Creation:");
  GECKO_INFO(app::platform_example::labels::Main,
             "    [1] Small fixed window       (400x300)");
  GECKO_INFO(app::platform_example::labels::Main,
             "    [2] Large resizable window   (1600x900)");
  GECKO_INFO(app::platform_example::labels::Main,
             "    [3] Borderless window        (800x600, no decorations)");
  GECKO_INFO(app::platform_example::labels::Main,
             "    [4] Borderless fullscreen window");
  GECKO_INFO(app::platform_example::labels::Main,
             "    [W] Close last spawned window");
  GECKO_INFO(app::platform_example::labels::Main, "");
  GECKO_INFO(app::platform_example::labels::Main,
             "  Main Window Manipulation:");
  GECKO_INFO(app::platform_example::labels::Main,
             "    [D] Toggle decorations on/off");
  GECKO_INFO(app::platform_example::labels::Main,
             "    [R] Toggle resizable on/off");
  GECKO_INFO(app::platform_example::labels::Main,
             "    [F] Toggle borderless fullscreen");
  GECKO_INFO(app::platform_example::labels::Main,
             "    [T] Toggle always-on-top");
  GECKO_INFO(app::platform_example::labels::Main,
             "    [M] Cycle window state (Normal→Maximized→Minimized)");
  GECKO_INFO(app::platform_example::labels::Main, "");
  GECKO_INFO(app::platform_example::labels::Main,
             "  Title-bar Button Control:");
  GECKO_INFO(app::platform_example::labels::Main,
             "    [5] Toggle close button");
  GECKO_INFO(app::platform_example::labels::Main,
             "    [6] Toggle minimize button");
  GECKO_INFO(app::platform_example::labels::Main,
             "    [7] Toggle maximize button");
  GECKO_INFO(app::platform_example::labels::Main,
             "    [8] Restore all buttons");
  GECKO_INFO(app::platform_example::labels::Main, "");
  GECKO_INFO(app::platform_example::labels::Main, "  Size Constraints:");
  GECKO_INFO(app::platform_example::labels::Main,
             "    [9] Set min size 400x300, max size 1920x1080");
  GECKO_INFO(app::platform_example::labels::Main,
             "    [0] Clear size constraints");
  GECKO_INFO(app::platform_example::labels::Main, "");
  GECKO_INFO(app::platform_example::labels::Main, "  Other:");
  GECKO_INFO(app::platform_example::labels::Main, "    [H] Print this help");
  GECKO_INFO(app::platform_example::labels::Main, "    [I] Print window info");
  GECKO_INFO(app::platform_example::labels::Main, "    [Escape] Quit");
  GECKO_INFO(app::platform_example::labels::Main,
             "═════════════════════════════════════════════════════");
}

const char* WindowModeToString(WindowMode mode) noexcept
{
  switch (mode)
  {
  case WindowMode::Windowed:
    return "Windowed";
  case WindowMode::Fullscreen:
    return "Fullscreen";
  case WindowMode::BorderlessFullscreen:
    return "BorderlessFullscreen";
  }
  return "Unknown";
}

const char* WindowStateToString(WindowState state) noexcept
{
  switch (state)
  {
  case WindowState::Normal:
    return "Normal";
  case WindowState::Minimized:
    return "Minimized";
  case WindowState::Maximized:
    return "Maximized";
  case WindowState::Hidden:
    return "Hidden";
  }
  return "Unknown";
}

const char* BoolStr(bool v) noexcept
{
  return v ? "true" : "false";
}

}  // namespace

int main()
{
  runtime::TrackingAllocator trackingAlloc;
  if (!SetAllocator(&trackingAlloc))
    return 1;

  runtime::RingProfiler ringProfiler(1 << 16);  // 64K events
  runtime::ImmediateLogger immediateLogger;     // Immediate logging

  runtime::EventBus eventBus;

  // Create job system with 4 worker threads
  runtime::ThreadPoolJobSystem jobSystem;
  jobSystem.SetWorkerThreadCount(4);

  runtime::RuntimeModule runtimeModule(jobSystem, ringProfiler, immediateLogger,
                                       eventBus);
  platform::PlatformModule platformModule;

  auto engine = Engine::Create({&runtimeModule, &platformModule, &g_AppModule});
  if (!engine)
  {
    ResetAllocator();
    return 1;
  }
  // Set up trace file sink for profiling data after services are available
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
    logger->SetLevel(LogLevel::Info);
  }
  {
    GECKO_FUNC(app::platform_example::labels::Main);
    GECKO_INFO(app::platform_example::labels::Main, gecko::VersionFullString());

    PlatformConfig cfg = {};
    cfg.Backend = DisplayBackendKind::Auto;

    PlatformContext ctx = PlatformContext(cfg);

    // ── Create main window — resizable, decorated ──────────────────
    WindowDesc windowDesc;
    windowDesc.Title = "Gecko Platform Example";
    windowDesc.Size = {1280, 720};
    windowDesc.Visible = true;
    windowDesc.Resizable = true;
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

    // ── Print initial window info ──────────────────────────────────
    {
      DpiInfo dpi = ctx.Windows().GetDpi(window);
      GECKO_INFO(app::platform_example::labels::Main,
                 "Window DPI: %u (scale %.2f)", dpi.Dpi,
                 static_cast<double>(dpi.Scale));

      Extent2D size = ctx.Windows().GetClientSize(window);
      GECKO_INFO(app::platform_example::labels::Main, "Client size: %ux%u",
                 size.Width, size.Height);

      math::Int2 pos = ctx.Windows().GetPosition(window);
      GECKO_INFO(app::platform_example::labels::Main,
                 "Window position: (%d, %d)", pos.X, pos.Y);

      NativeWindowHandle native = ctx.Windows().GetNativeWindowHandle(window);
      GECKO_INFO(app::platform_example::labels::Main,
                 "Native handle: backend=%u, handle=%p",
                 static_cast<unsigned>(native.Backend), native.Handle);

      GECKO_INFO(app::platform_example::labels::Main, "Decorated: %s",
                 BoolStr(ctx.Windows().IsDecorated(window)));
      GECKO_INFO(app::platform_example::labels::Main, "Resizable: %s",
                 BoolStr(ctx.Windows().IsResizable(window)));
      GECKO_INFO(app::platform_example::labels::Main, "Window mode: %s",
                 WindowModeToString(ctx.Windows().GetWindowMode(window)));
      GECKO_INFO(app::platform_example::labels::Main, "Window state: %s",
                 WindowStateToString(ctx.Windows().GetWindowState(window)));
      GECKO_INFO(app::platform_example::labels::Main, "Always on top: %s",
                 BoolStr(ctx.Windows().IsAlwaysOnTop(window)));
    }

    // ── App state for event callbacks ───────────────────────────────
    struct AppState
    {
      PlatformContext* Ctx;
      WindowHandle MainWindow;
      ::std::vector<WindowHandle> Spawned;
      bool Running {true};
    };

    AppState appState;
    appState.Ctx = &ctx;
    appState.MainWindow = window;

    // ── Print keybindings ─────────────────────────────────────────
    PrintHelp();

    // ── Close handler ──────────────────────────────────────────────
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

          // Child window closed — remove from spawned list.
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

    // ── Key handler ────────────────────────────────────────────────
    auto keyPressedSub = gecko::SubscribeEvent(
        events::WindowKey,
        [](void* user, const gecko::EventMeta& /*meta*/,
           gecko::EventView view) {
          auto* state = static_cast<AppState*>(user);
          const auto* payload =
              reinterpret_cast<const events::WindowKeyPayload*>(view.Data());

          if (!payload->Down || payload->Repeat)
            return;

          auto& windows = state->Ctx->Windows();
          const WindowHandle main = state->MainWindow;

          switch (payload->Key)
          {
          // ── Window creation ────────────────────────────────────
          case KeyCode::D1: {
            WindowDesc desc;
            desc.Title = "Gecko - Small Fixed";
            desc.Size = {400, 300};
            desc.Resizable = false;
            desc.Mode = WindowMode::Windowed;

            WindowHandle h = windows.CreateWindow(desc);
            if (h.IsValid())
            {
              state->Spawned.push_back(h);
              GECKO_INFO(app::platform_example::labels::Main,
                         "Opened small fixed window (id=%llu, 400x300)",
                         static_cast<unsigned long long>(h.Id));
            }
            break;
          }

          case KeyCode::D2: {
            WindowDesc desc;
            desc.Title = "Gecko - Large Resizable";
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
            break;
          }

          case KeyCode::D3: {
            WindowDesc desc;
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
            break;
          }

          case KeyCode::D4: {
            WindowDesc desc;
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

          // ── Main window manipulation ───────────────────────────
          case KeyCode::D: {
            bool decorated = windows.IsDecorated(main);
            windows.SetDecorated(main, !decorated);
            GECKO_INFO(app::platform_example::labels::Main,
                       "Decorations: %s → %s", BoolStr(decorated),
                       BoolStr(!decorated));
            break;
          }

          case KeyCode::R: {
            bool resizable = windows.IsResizable(main);
            windows.SetResizable(main, !resizable);
            GECKO_INFO(app::platform_example::labels::Main,
                       "Resizable: %s → %s", BoolStr(resizable),
                       BoolStr(!resizable));
            break;
          }

          case KeyCode::F: {
            WindowMode mode = windows.GetWindowMode(main);
            WindowMode next = (mode == WindowMode::Windowed)
                                  ? WindowMode::BorderlessFullscreen
                                  : WindowMode::Windowed;
            windows.SetWindowMode(main, next);
            GECKO_INFO(app::platform_example::labels::Main,
                       "Window mode: %s → %s", WindowModeToString(mode),
                       WindowModeToString(next));
            break;
          }

          case KeyCode::T: {
            bool topmost = windows.IsAlwaysOnTop(main);
            windows.SetAlwaysOnTop(main, !topmost);
            GECKO_INFO(app::platform_example::labels::Main,
                       "Always on top: %s → %s", BoolStr(topmost),
                       BoolStr(!topmost));
            break;
          }

          case KeyCode::M: {
            WindowState st = windows.GetWindowState(main);
            WindowState next;
            switch (st)
            {
            case WindowState::Normal:
              next = WindowState::Maximized;
              break;
            case WindowState::Maximized:
              next = WindowState::Minimized;
              break;
            default:
              next = WindowState::Normal;
              break;
            }
            windows.SetWindowState(main, next);
            GECKO_INFO(app::platform_example::labels::Main,
                       "Window state: %s → %s", WindowStateToString(st),
                       WindowStateToString(next));
            break;
          }

          // ── Title-bar button control ───────────────────────────
          case KeyCode::D5: {
            WindowButtons btns = windows.GetWindowButtons(main);
            WindowButtons next = Any(btns & WindowButtons::Close)
                                     ? (btns & ~WindowButtons::Close)
                                     : (btns | WindowButtons::Close);
            windows.SetWindowButtons(main, next);
            GECKO_INFO(app::platform_example::labels::Main, "Close button: %s",
                       BoolStr(Any(next & WindowButtons::Close)));
            break;
          }

          case KeyCode::D6: {
            WindowButtons btns = windows.GetWindowButtons(main);
            WindowButtons next = Any(btns & WindowButtons::Minimize)
                                     ? (btns & ~WindowButtons::Minimize)
                                     : (btns | WindowButtons::Minimize);
            windows.SetWindowButtons(main, next);
            GECKO_INFO(app::platform_example::labels::Main,
                       "Minimize button: %s",
                       BoolStr(Any(next & WindowButtons::Minimize)));
            break;
          }

          case KeyCode::D7: {
            WindowButtons btns = windows.GetWindowButtons(main);
            WindowButtons next = Any(btns & WindowButtons::Maximize)
                                     ? (btns & ~WindowButtons::Maximize)
                                     : (btns | WindowButtons::Maximize);
            windows.SetWindowButtons(main, next);
            GECKO_INFO(app::platform_example::labels::Main,
                       "Maximize button: %s",
                       BoolStr(Any(next & WindowButtons::Maximize)));
            break;
          }

          case KeyCode::D8: {
            windows.SetWindowButtons(main, WindowButtons::All);
            GECKO_INFO(app::platform_example::labels::Main,
                       "Restored all title-bar buttons");
            break;
          }

          // ── Size constraints ───────────────────────────────────
          case KeyCode::D9: {
            windows.SetMinSize(main, {400, 300});
            windows.SetMaxSize(main, {1920, 1080});
            GECKO_INFO(app::platform_example::labels::Main,
                       "Size constraints: min=400x300, max=1920x1080");
            break;
          }

          case KeyCode::D0: {
            windows.SetMinSize(main, {0, 0});
            windows.SetMaxSize(main, {0, 0});
            GECKO_INFO(app::platform_example::labels::Main,
                       "Size constraints cleared");
            break;
          }

          // ── Help / Info ────────────────────────────────────────
          case KeyCode::H: {
            PrintHelp();
            break;
          }

          case KeyCode::I: {
            DpiInfo dpi = windows.GetDpi(main);
            Extent2D size = windows.GetClientSize(main);
            math::Int2 pos = windows.GetPosition(main);

            GECKO_INFO(app::platform_example::labels::Main,
                       "── Window Info ──────────────────────────");
            GECKO_INFO(app::platform_example::labels::Main,
                       "  Size:          %ux%u", size.Width, size.Height);
            GECKO_INFO(app::platform_example::labels::Main,
                       "  Position:      (%d, %d)", pos.X, pos.Y);
            GECKO_INFO(app::platform_example::labels::Main,
                       "  DPI:           %u (scale %.2f)", dpi.Dpi,
                       static_cast<double>(dpi.Scale));
            GECKO_INFO(app::platform_example::labels::Main,
                       "  Mode:          %s",
                       WindowModeToString(windows.GetWindowMode(main)));
            GECKO_INFO(app::platform_example::labels::Main,
                       "  State:         %s",
                       WindowStateToString(windows.GetWindowState(main)));
            GECKO_INFO(app::platform_example::labels::Main,
                       "  Decorated:     %s",
                       BoolStr(windows.IsDecorated(main)));
            GECKO_INFO(app::platform_example::labels::Main,
                       "  Resizable:     %s",
                       BoolStr(windows.IsResizable(main)));
            GECKO_INFO(app::platform_example::labels::Main,
                       "  Always on top: %s",
                       BoolStr(windows.IsAlwaysOnTop(main)));

            WindowButtons btns = windows.GetWindowButtons(main);
            GECKO_INFO(app::platform_example::labels::Main,
                       "  Buttons:       close=%s, min=%s, max=%s",
                       BoolStr(Any(btns & WindowButtons::Close)),
                       BoolStr(Any(btns & WindowButtons::Minimize)),
                       BoolStr(Any(btns & WindowButtons::Maximize)));
            GECKO_INFO(app::platform_example::labels::Main,
                       "─────────────────────────────────────────");
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

    auto stateChangedSub = gecko::SubscribeEvent(
        events::WindowStateChanged,
        [](void* /*user*/, const gecko::EventMeta& /*meta*/,
           gecko::EventView view) {
          const auto* payload =
              reinterpret_cast<const events::WindowStateChangedPayload*>(
                  view.Data());
          GECKO_INFO(app::platform_example::labels::Main,
                     "WindowStateChanged: windowId=%llu, %s → %s",
                     static_cast<unsigned long long>(payload->Window.Id),
                     WindowStateToString(payload->OldState),
                     WindowStateToString(payload->NewState));
        },
        nullptr);

    // ── Frame callback (shared between normal loop and modal timer) ──
    auto doFrame = [&]() {
      GECKO_SCOPE_NAMED(app::platform_example::labels::Main, "Frame");
      {
        GECKO_SCOPE_NAMED(app::platform_example::labels::Main,
                          "DispatchEvents");
        (void)gecko::DispatchEvents();
      }
      GECKO_PRECISE_SLEEP_NS(16'000'000);
    };

    // Register for Win32 modal drag/resize so the app keeps ticking.
    ctx.SetModalFrameCallback(
        [](void* ud) { (*static_cast<decltype(&doFrame)>(ud))(); }, &doFrame);

    // ── Main loop (runs until user quits) ──────────────────────────
    GECKO_INFO(app::platform_example::labels::Main, "Entering main loop...");
    while (appState.Running && ctx.Windows().IsWindowAlive(window))
    {
      GECKO_SCOPE_NAMED(app::platform_example::labels::Main, "MainLoop");

      {
        GECKO_SCOPE_NAMED(app::platform_example::labels::Main, "PumpEvents");
        ctx.PumpEvents();
      }

      doFrame();
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

  engine.reset();
  ResetAllocator();

  ::std::printf("Application exited successfully\n");
  return 0;
}
