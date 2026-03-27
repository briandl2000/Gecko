#include "gecko/core/services.h"
#include "gecko/platform/platform_config.h"
#include "gecko/platform/platform_context.h"
#include "gecko/platform/platform_events.h"
#include "gecko/runtime/event_bus.h"

#include <catch2/catch_test_macros.hpp>

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
    alloc.Init();
    jobs.Init();
    profiler.Init();
    logger.Init();
    (void)modules.Init();
    (void)events.Init();

    Services svc {
        .Allocator = &alloc,
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
  }
};

}  // namespace

TEST_CASE("Null backend: create and destroy window", "[platform][context]")
{
  TestServiceScope scope;

  PlatformConfig cfg = {};
  cfg.Backend = DisplayBackendKind::Null;
  auto ctx = PlatformContext(cfg);

  WindowHandle win;
  REQUIRE(ctx.Windows().CreateWindow({}, win));
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
  WindowHandle win;
  REQUIRE(ctx.Windows().CreateWindow(desc, win));

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

  WindowHandle win;
  ctx.Windows().CreateWindow({}, win);
  ctx.Windows().SetTitle(win, "Test Title");
  ctx.Windows().DestroyWindow(win);
}

TEST_CASE("Null backend: request close enqueues event", "[platform][context]")
{
  TestServiceScope scope;

  PlatformConfig cfg = {};
  cfg.Backend = DisplayBackendKind::Null;
  auto ctx = PlatformContext(cfg);

  WindowHandle win;
  ctx.Windows().CreateWindow({}, win);
  REQUIRE(ctx.Windows().RequestClose(win));

  int received = 0;
  auto sub = gecko::SubscribeEvent(
      events::WindowCloseRequested,
      [](void* user, const gecko::EventMeta&, gecko::EventView) {
        (*static_cast<int*>(user))++;
      },
      &received);

  ctx.PumpEvents();
  (void)gecko::DispatchQueuedEvents();

  REQUIRE(received == 1);

  ctx.Windows().DestroyWindow(win);
}

TEST_CASE("Null backend: multiple windows", "[platform][context]")
{
  TestServiceScope scope;

  PlatformConfig cfg = {};
  cfg.Backend = DisplayBackendKind::Null;
  auto ctx = PlatformContext(cfg);

  WindowHandle w1, w2, w3;
  REQUIRE(ctx.Windows().CreateWindow({}, w1));
  REQUIRE(ctx.Windows().CreateWindow({}, w2));
  REQUIRE(ctx.Windows().CreateWindow({}, w3));

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

  WindowHandle win;
  ctx.Windows().CreateWindow({}, win);

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

  MonitorHandle primary;
  REQUIRE(ctx.Monitors().GetPrimaryMonitor(primary));
  REQUIRE(primary.IsValid());
}

TEST_CASE("Null monitor backend: get handle by index", "[platform][context]")
{
  TestServiceScope scope;

  PlatformConfig cfg = {};
  cfg.Backend = DisplayBackendKind::Null;
  auto ctx = PlatformContext(cfg);

  MonitorHandle h;
  REQUIRE(ctx.Monitors().GetMonitorHandle(0, h));
  REQUIRE(h.IsValid());

  MonitorHandle invalid;
  REQUIRE_FALSE(ctx.Monitors().GetMonitorHandle(99, invalid));
}

TEST_CASE("Null monitor backend: monitor properties are valid",
          "[platform][context]")
{
  TestServiceScope scope;

  PlatformConfig cfg = {};
  cfg.Backend = DisplayBackendKind::Null;
  auto ctx = PlatformContext(cfg);

  MonitorHandle h;
  REQUIRE(ctx.Monitors().GetMonitorHandle(0, h));

  MonitorInfo info {};
  REQUIRE(ctx.Monitors().GetMonitorProperties(h, info));
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

  MonitorHandle h;
  REQUIRE(ctx.Monitors().GetMonitorHandle(0, h));

  math::Rect2D bounds {};
  math::Rect2D workArea {};
  REQUIRE(ctx.Monitors().GetMonitorBounds(h, bounds, workArea));
  REQUIRE_FALSE(bounds.IsEmpty());
  REQUIRE_FALSE(workArea.IsEmpty());
}

TEST_CASE("Null monitor backend: invalid handle returns false",
          "[platform][context]")
{
  TestServiceScope scope;

  PlatformConfig cfg = {};
  cfg.Backend = DisplayBackendKind::Null;
  auto ctx = PlatformContext(cfg);

  MonitorHandle invalid;
  MonitorInfo info {};
  REQUIRE_FALSE(ctx.Monitors().GetMonitorProperties(invalid, info));

  math::Rect2D bounds {};
  math::Rect2D workArea {};
  REQUIRE_FALSE(ctx.Monitors().GetMonitorBounds(invalid, bounds, workArea));
}

// ── Event delivery tests ───────────────────────────────────────────────

TEST_CASE("Null backend: destroy window enqueues WindowClosed event",
          "[platform][context]")
{
  TestServiceScope scope;

  PlatformConfig cfg = {};
  cfg.Backend = DisplayBackendKind::Null;
  auto ctx = PlatformContext(cfg);

  WindowHandle win;
  ctx.Windows().CreateWindow({}, win);

  int received = 0;
  auto sub = gecko::SubscribeEvent(
      events::WindowClosed,
      [](void* user, const gecko::EventMeta&, gecko::EventView) {
        (*static_cast<int*>(user))++;
      },
      &received);

  ctx.Windows().DestroyWindow(win);
  ctx.PumpEvents();
  (void)gecko::DispatchQueuedEvents();

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
