#include "gecko/core/services.h"
#include "gecko/platform/platform_config.h"
#include "gecko/platform/platform_context.h"

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
  NullEventBus events;

  TestServiceScope()
  {
    alloc.Init();
    jobs.Init();
    profiler.Init();
    logger.Init();
    (void)modules.Init();
    events.Init();

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

  WindowEvent ev;
  REQUIRE(ctx.Windows().PollEvent(ev));
  REQUIRE(ev.Kind == WindowEventKind::CloseRequested);
  REQUIRE(ev.Window == win);

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

  ctx.Windows().PumpEvents();
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
