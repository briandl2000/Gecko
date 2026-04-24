#include "gecko/core/services.h"
#include "gecko/platform/platform_config.h"
#include "gecko/platform/platform_context.h"
#include "gecko/platform/platform_events.h"
#include "gecko/runtime/event_bus.h"

#include <catch2/catch_test_macros.hpp>
#include <cstring>

using namespace gecko;
using namespace gecko::platform;

namespace {

struct TestServiceScope
{
  SystemAllocator alloc;
  NullJobSystem jobs;
  NullProfiler profiler;
  NullLogger logger;
  NullModuleRegistry modules;
  runtime::EventBus events;

  TestServiceScope()
  {
    (void)SetAllocator(&alloc);
    jobs.Init();
    profiler.Init();
    logger.Init();
    (void)modules.Init();
    (void)events.Init();

    Services svc {
        .JobSystem = &jobs,
        .Profiler = &profiler,
        .Logger = &logger,
        .Modules = &modules,
        .EventBus = &events,
    };
    (void)InstallServices(svc);
  }

  ~TestServiceScope()
  {
    UninstallServices();
    ResetAllocator();
  }
};

}  // namespace

TEST_CASE("Null backend: create and destroy window", "[platform][context]")
{
  TestServiceScope scope;

  PlatformConfig cfg = {};
  cfg.Backend = DisplayBackendKind::Null;
  auto ctx = PlatformContext(cfg);

  WindowHandle win = ctx.Windows().CreateWindow({});
  REQUIRE(win.IsValid());
  REQUIRE(ctx.Windows().IsWindowAlive(win));

  ctx.Windows().DestroyWindow(win);
  REQUIRE_FALSE(ctx.Windows().IsWindowAlive(win));
}

TEST_CASE("Null backend: client size matches desc", "[platform][context]")
{
  TestServiceScope scope;

  PlatformConfig cfg = {};
  cfg.Backend = DisplayBackendKind::Null;
  auto ctx = PlatformContext(cfg);

  WindowDesc desc;
  desc.Size = {800, 600};
  WindowHandle win = ctx.Windows().CreateWindow(desc);
  REQUIRE(win.IsValid());

  Extent2D size = ctx.Windows().GetClientSize(win);
  REQUIRE(size.Width == 800);
  REQUIRE(size.Height == 600);

  ctx.Windows().DestroyWindow(win);
}

TEST_CASE("Null backend: set title doesn't crash", "[platform][context]")
{
  TestServiceScope scope;

  PlatformConfig cfg = {};
  cfg.Backend = DisplayBackendKind::Null;
  auto ctx = PlatformContext(cfg);

  WindowHandle win = ctx.Windows().CreateWindow({});
  ctx.Windows().SetTitle(win, "Test Title");
  ctx.Windows().DestroyWindow(win);
}

TEST_CASE("Null backend: request close enqueues event", "[platform][context]")
{
  TestServiceScope scope;

  PlatformConfig cfg = {};
  cfg.Backend = DisplayBackendKind::Null;
  auto ctx = PlatformContext(cfg);

  WindowHandle win = ctx.Windows().CreateWindow({});
  REQUIRE(ctx.Windows().RequestClose(win));

  int received = 0;
  auto sub = gecko::SubscribeEvent(
      events::WindowCloseRequested,
      [](void* user, const gecko::EventMeta&, gecko::EventView) {
        (*static_cast<int*>(user))++;
      },
      &received);

  ctx.PumpEvents();
  (void)gecko::DispatchEvents();

  REQUIRE(received == 1);

  ctx.Windows().DestroyWindow(win);
}

TEST_CASE("Null backend: multiple windows", "[platform][context]")
{
  TestServiceScope scope;

  PlatformConfig cfg = {};
  cfg.Backend = DisplayBackendKind::Null;
  auto ctx = PlatformContext(cfg);

  WindowHandle w1 = ctx.Windows().CreateWindow({});
  WindowHandle w2 = ctx.Windows().CreateWindow({});
  WindowHandle w3 = ctx.Windows().CreateWindow({});
  REQUIRE(w1.IsValid());
  REQUIRE(w2.IsValid());
  REQUIRE(w3.IsValid());

  REQUIRE(w1 != w2);
  REQUIRE(w2 != w3);

  REQUIRE(ctx.Windows().IsWindowAlive(w1));
  REQUIRE(ctx.Windows().IsWindowAlive(w2));
  REQUIRE(ctx.Windows().IsWindowAlive(w3));

  ctx.Windows().DestroyWindow(w2);
  REQUIRE(ctx.Windows().IsWindowAlive(w1));
  REQUIRE_FALSE(ctx.Windows().IsWindowAlive(w2));
  REQUIRE(ctx.Windows().IsWindowAlive(w3));

  ctx.Windows().DestroyWindow(w1);
  ctx.Windows().DestroyWindow(w3);
}

TEST_CASE("Null backend: DPI info has defaults", "[platform][context]")
{
  TestServiceScope scope;

  PlatformConfig cfg = {};
  cfg.Backend = DisplayBackendKind::Null;
  auto ctx = PlatformContext(cfg);

  WindowHandle win = ctx.Windows().CreateWindow({});

  DpiInfo dpi = ctx.Windows().GetDpi(win);
  REQUIRE(dpi.Dpi > 0);
  REQUIRE(dpi.Scale > 0.0f);

  ctx.Windows().DestroyWindow(win);
}

TEST_CASE("Null backend: PumpEvents doesn't crash", "[platform][context]")
{
  TestServiceScope scope;

  PlatformConfig cfg = {};
  cfg.Backend = DisplayBackendKind::Null;
  auto ctx = PlatformContext(cfg);

  ctx.PumpEvents();
}

TEST_CASE("Null backend: invalid window operations are safe",
          "[platform][context]")
{
  TestServiceScope scope;

  PlatformConfig cfg = {};
  cfg.Backend = DisplayBackendKind::Null;
  auto ctx = PlatformContext(cfg);

  WindowHandle invalid;
  REQUIRE_FALSE(ctx.Windows().IsWindowAlive(invalid));
  REQUIRE_FALSE(ctx.Windows().RequestClose(invalid));
  ctx.Windows().DestroyWindow(invalid);
}

TEST_CASE("Null backend: set and get client size", "[platform][context]")
{
  TestServiceScope scope;

  PlatformConfig cfg = {};
  cfg.Backend = DisplayBackendKind::Null;
  auto ctx = PlatformContext(cfg);

  WindowHandle win = ctx.Windows().CreateWindow({});

  ctx.Windows().SetClientSize(win, {1024, 768});
  Extent2D size = ctx.Windows().GetClientSize(win);
  REQUIRE(size.Width == 1024);
  REQUIRE(size.Height == 768);

  ctx.Windows().DestroyWindow(win);
}

TEST_CASE("Null backend: get title returns desc title", "[platform][context]")
{
  TestServiceScope scope;

  PlatformConfig cfg = {};
  cfg.Backend = DisplayBackendKind::Null;
  auto ctx = PlatformContext(cfg);

  WindowDesc desc;
  desc.Title = "My Window";
  WindowHandle win = ctx.Windows().CreateWindow(desc);

  const char* title = ctx.Windows().GetTitle(win);
  REQUIRE(::std::strcmp(title, "My Window") == 0);

  ctx.Windows().SetTitle(win, "Updated");
  REQUIRE(::std::strcmp(ctx.Windows().GetTitle(win), "Updated") == 0);

  ctx.Windows().DestroyWindow(win);
}

TEST_CASE("Null backend: set and get position", "[platform][context]")
{
  TestServiceScope scope;

  PlatformConfig cfg = {};
  cfg.Backend = DisplayBackendKind::Null;
  auto ctx = PlatformContext(cfg);

  WindowHandle win = ctx.Windows().CreateWindow({});

  ctx.Windows().SetPosition(win, {100, 200});
  auto pos = ctx.Windows().GetPosition(win);
  REQUIRE(pos.X == 100);
  REQUIRE(pos.Y == 200);

  ctx.Windows().DestroyWindow(win);
}

TEST_CASE("Null backend: set position fires WindowMoved event",
          "[platform][context]")
{
  TestServiceScope scope;

  PlatformConfig cfg = {};
  cfg.Backend = DisplayBackendKind::Null;
  auto ctx = PlatformContext(cfg);

  WindowHandle win = ctx.Windows().CreateWindow({});

  int received = 0;
  auto sub = gecko::SubscribeEvent(
      events::WindowMoved,
      [](void* user, const gecko::EventMeta&, gecko::EventView) {
        (*static_cast<int*>(user))++;
      },
      &received);

  ctx.Windows().SetPosition(win, {50, 75});
  ctx.PumpEvents();
  (void)gecko::DispatchEvents();

  REQUIRE(received == 1);

  ctx.Windows().DestroyWindow(win);
}

TEST_CASE("Null backend: set and get window state", "[platform][context]")
{
  TestServiceScope scope;

  PlatformConfig cfg = {};
  cfg.Backend = DisplayBackendKind::Null;
  auto ctx = PlatformContext(cfg);

  WindowHandle win = ctx.Windows().CreateWindow({});

  REQUIRE(ctx.Windows().GetWindowState(win) == WindowState::Normal);

  ctx.Windows().SetWindowState(win, WindowState::Minimized);
  REQUIRE(ctx.Windows().GetWindowState(win) == WindowState::Minimized);

  ctx.Windows().SetWindowState(win, WindowState::Maximized);
  REQUIRE(ctx.Windows().GetWindowState(win) == WindowState::Maximized);

  ctx.Windows().DestroyWindow(win);
}

TEST_CASE("Null backend: set state fires WindowStateChanged event",
          "[platform][context]")
{
  TestServiceScope scope;

  PlatformConfig cfg = {};
  cfg.Backend = DisplayBackendKind::Null;
  auto ctx = PlatformContext(cfg);

  WindowHandle win = ctx.Windows().CreateWindow({});

  int received = 0;
  auto sub = gecko::SubscribeEvent(
      events::WindowStateChanged,
      [](void* user, const gecko::EventMeta&, gecko::EventView) {
        (*static_cast<int*>(user))++;
      },
      &received);

  ctx.Windows().SetWindowState(win, WindowState::Hidden);
  ctx.PumpEvents();
  (void)gecko::DispatchEvents();

  REQUIRE(received == 1);

  ctx.Windows().DestroyWindow(win);
}

TEST_CASE("Null backend: decorated get/set", "[platform][context]")
{
  TestServiceScope scope;

  PlatformConfig cfg = {};
  cfg.Backend = DisplayBackendKind::Null;
  auto ctx = PlatformContext(cfg);

  WindowHandle win = ctx.Windows().CreateWindow({});

  REQUIRE(ctx.Windows().IsDecorated(win) == true);

  ctx.Windows().SetDecorated(win, false);
  REQUIRE(ctx.Windows().IsDecorated(win) == false);

  ctx.Windows().SetDecorated(win, true);
  REQUIRE(ctx.Windows().IsDecorated(win) == true);

  ctx.Windows().DestroyWindow(win);
}

TEST_CASE("Null backend: cursor mode get/set", "[platform][context]")
{
  TestServiceScope scope;

  PlatformConfig cfg = {};
  cfg.Backend = DisplayBackendKind::Null;
  auto ctx = PlatformContext(cfg);

  WindowHandle win = ctx.Windows().CreateWindow({});

  REQUIRE(ctx.Windows().GetCursorMode(win) == CursorMode::Normal);

  ctx.Windows().SetCursorMode(win, CursorMode::Hidden);
  REQUIRE(ctx.Windows().GetCursorMode(win) == CursorMode::Hidden);

  ctx.Windows().SetCursorMode(win, CursorMode::Locked);
  REQUIRE(ctx.Windows().GetCursorMode(win) == CursorMode::Locked);

  ctx.Windows().DestroyWindow(win);
}

TEST_CASE("Null backend: request focus does not crash", "[platform][context]")
{
  TestServiceScope scope;

  PlatformConfig cfg = {};
  cfg.Backend = DisplayBackendKind::Null;
  auto ctx = PlatformContext(cfg);

  WindowHandle win = ctx.Windows().CreateWindow({});

  ctx.Windows().RequestFocus(win);

  ctx.Windows().DestroyWindow(win);
}

TEST_CASE("Null backend: hidden window has Hidden state", "[platform][context]")
{
  TestServiceScope scope;

  PlatformConfig cfg = {};
  cfg.Backend = DisplayBackendKind::Null;
  auto ctx = PlatformContext(cfg);

  WindowDesc desc;
  desc.Visible = false;
  WindowHandle win = ctx.Windows().CreateWindow(desc);

  REQUIRE(ctx.Windows().GetWindowState(win) == WindowState::Hidden);

  ctx.Windows().DestroyWindow(win);
}

// ── Monitor backend tests ──────────────────────────────────────────────

TEST_CASE("Null monitor backend: enumerates one virtual monitor",
          "[platform][context]")
{
  TestServiceScope scope;

  PlatformConfig cfg = {};
  cfg.Backend = DisplayBackendKind::Null;
  auto ctx = PlatformContext(cfg);

  REQUIRE(ctx.Monitors().GetMonitorCount() == 1);
}

TEST_CASE("Null monitor backend: primary monitor exists", "[platform][context]")
{
  TestServiceScope scope;

  PlatformConfig cfg = {};
  cfg.Backend = DisplayBackendKind::Null;
  auto ctx = PlatformContext(cfg);

  MonitorHandle primary = ctx.Monitors().GetPrimaryMonitor();
  REQUIRE(primary.IsValid());
}

TEST_CASE("Null monitor backend: get handle by index", "[platform][context]")
{
  TestServiceScope scope;

  PlatformConfig cfg = {};
  cfg.Backend = DisplayBackendKind::Null;
  auto ctx = PlatformContext(cfg);

  MonitorHandle h = ctx.Monitors().GetMonitorHandle(0);
  REQUIRE(h.IsValid());

  MonitorHandle invalid = ctx.Monitors().GetMonitorHandle(99);
  REQUIRE_FALSE(invalid.IsValid());
}

TEST_CASE("Null monitor backend: monitor properties are valid",
          "[platform][context]")
{
  TestServiceScope scope;

  PlatformConfig cfg = {};
  cfg.Backend = DisplayBackendKind::Null;
  auto ctx = PlatformContext(cfg);

  MonitorHandle h = ctx.Monitors().GetMonitorHandle(0);

  MonitorInfo info = ctx.Monitors().GetMonitorProperties(h);
  REQUIRE(info.IsPrimary);
  REQUIRE(info.Dpi > 0);
  REQUIRE(info.RefreshRateMilliHz > 0);
  REQUIRE_FALSE(info.Bounds.IsEmpty());
}

TEST_CASE("Null monitor backend: bounds and work area", "[platform][context]")
{
  TestServiceScope scope;

  PlatformConfig cfg = {};
  cfg.Backend = DisplayBackendKind::Null;
  auto ctx = PlatformContext(cfg);

  MonitorHandle h = ctx.Monitors().GetMonitorHandle(0);

  MonitorBounds mb = ctx.Monitors().GetMonitorBounds(h);
  REQUIRE_FALSE(mb.Bounds.IsEmpty());
  REQUIRE_FALSE(mb.WorkArea.IsEmpty());
}

TEST_CASE("Null monitor backend: invalid handle returns false",
          "[platform][context]")
{
  TestServiceScope scope;

  PlatformConfig cfg = {};
  cfg.Backend = DisplayBackendKind::Null;
  auto ctx = PlatformContext(cfg);

  MonitorHandle invalid;
  MonitorInfo info = ctx.Monitors().GetMonitorProperties(invalid);
  REQUIRE(info.Bounds.IsEmpty());

  MonitorBounds mb = ctx.Monitors().GetMonitorBounds(invalid);
  REQUIRE(mb.Bounds.IsEmpty());
  REQUIRE(mb.WorkArea.IsEmpty());
}

// ── Event delivery tests ───────────────────────────────────────────────

TEST_CASE("Null backend: destroy window enqueues WindowClosed event",
          "[platform][context]")
{
  TestServiceScope scope;

  PlatformConfig cfg = {};
  cfg.Backend = DisplayBackendKind::Null;
  auto ctx = PlatformContext(cfg);

  WindowHandle win = ctx.Windows().CreateWindow({});

  int received = 0;
  auto sub = gecko::SubscribeEvent(
      events::WindowClosed,
      [](void* user, const gecko::EventMeta&, gecko::EventView) {
        (*static_cast<int*>(user))++;
      },
      &received);

  ctx.Windows().DestroyWindow(win);
  ctx.PumpEvents();
  (void)gecko::DispatchEvents();

  REQUIRE(received == 1);
}

// ── Config resolution tests ────────────────────────────────────────────

TEST_CASE("Resolve: explicit backend is preserved", "[platform][config]")
{
  TestServiceScope scope;

  PlatformConfig cfg = {};
  cfg.Backend = DisplayBackendKind::Null;
  PlatformConfig resolved = Resolve(cfg);
  REQUIRE(resolved.Backend == DisplayBackendKind::Null);
}

TEST_CASE("Resolve: Auto resolves to concrete backend", "[platform][config]")
{
  TestServiceScope scope;

  PlatformConfig cfg = {};
  cfg.Backend = DisplayBackendKind::Auto;
  PlatformConfig resolved = Resolve(cfg);
  REQUIRE(resolved.Backend != DisplayBackendKind::Auto);
}

TEST_CASE("PlatformContext stores resolved config", "[platform][context]")
{
  TestServiceScope scope;

  PlatformConfig cfg = {};
  cfg.Backend = DisplayBackendKind::Null;
  auto ctx = PlatformContext(cfg);
  REQUIRE(ctx.Config().Backend == DisplayBackendKind::Null);
}
