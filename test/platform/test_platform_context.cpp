#include "gecko/core/services.h"
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

TEST_CASE("PlatformContext::Create with Null backend", "[platform][context]")
{
  TestServiceScope scope;

  PlatformConfig cfg {.WindowBackend = WindowBackendKind::Null};
  auto ctx = PlatformContext::Create(cfg);
  REQUIRE(ctx != nullptr);
}

TEST_CASE("Null backend: create and destroy window", "[platform][context]")
{
  TestServiceScope scope;

  auto ctx =
      PlatformContext::Create({.WindowBackend = WindowBackendKind::Null});
  REQUIRE(ctx != nullptr);

  WindowHandle win;
  REQUIRE(ctx->CreateWindow({}, win));
  REQUIRE(win.IsValid());
  REQUIRE(ctx->IsWindowAlive(win));

  ctx->DestroyWindow(win);
  REQUIRE_FALSE(ctx->IsWindowAlive(win));
}

TEST_CASE("Null backend: client size matches desc", "[platform][context]")
{
  TestServiceScope scope;

  auto ctx =
      PlatformContext::Create({.WindowBackend = WindowBackendKind::Null});

  WindowDesc desc;
  desc.Size = {800, 600};
  WindowHandle win;
  REQUIRE(ctx->CreateWindow(desc, win));

  Extent2D size = ctx->GetClientSize(win);
  REQUIRE(size.Width == 800);
  REQUIRE(size.Height == 600);

  ctx->DestroyWindow(win);
}

TEST_CASE("Null backend: set title doesn't crash", "[platform][context]")
{
  TestServiceScope scope;

  auto ctx =
      PlatformContext::Create({.WindowBackend = WindowBackendKind::Null});

  WindowHandle win;
  ctx->CreateWindow({}, win);
  ctx->SetTitle(win, "Test Title");
  ctx->DestroyWindow(win);
}

TEST_CASE("Null backend: request close enqueues event", "[platform][context]")
{
  TestServiceScope scope;

  auto ctx =
      PlatformContext::Create({.WindowBackend = WindowBackendKind::Null});

  WindowHandle win;
  ctx->CreateWindow({}, win);
  REQUIRE(ctx->RequestClose(win));

  WindowEvent ev;
  REQUIRE(ctx->PollEvent(ev));
  REQUIRE(ev.Kind == WindowEventKind::CloseRequested);
  REQUIRE(ev.Window == win);

  ctx->DestroyWindow(win);
}

TEST_CASE("Null backend: multiple windows", "[platform][context]")
{
  TestServiceScope scope;

  auto ctx =
      PlatformContext::Create({.WindowBackend = WindowBackendKind::Null});

  WindowHandle w1, w2, w3;
  REQUIRE(ctx->CreateWindow({}, w1));
  REQUIRE(ctx->CreateWindow({}, w2));
  REQUIRE(ctx->CreateWindow({}, w3));

  REQUIRE(w1 != w2);
  REQUIRE(w2 != w3);

  REQUIRE(ctx->IsWindowAlive(w1));
  REQUIRE(ctx->IsWindowAlive(w2));
  REQUIRE(ctx->IsWindowAlive(w3));

  ctx->DestroyWindow(w2);
  REQUIRE(ctx->IsWindowAlive(w1));
  REQUIRE_FALSE(ctx->IsWindowAlive(w2));
  REQUIRE(ctx->IsWindowAlive(w3));

  ctx->DestroyWindow(w1);
  ctx->DestroyWindow(w3);
}

TEST_CASE("Null backend: DPI info has defaults", "[platform][context]")
{
  TestServiceScope scope;

  auto ctx =
      PlatformContext::Create({.WindowBackend = WindowBackendKind::Null});

  WindowHandle win;
  ctx->CreateWindow({}, win);

  DpiInfo dpi = ctx->GetDpi(win);
  REQUIRE(dpi.Dpi > 0);
  REQUIRE(dpi.Scale > 0.0f);

  ctx->DestroyWindow(win);
}

TEST_CASE("Null backend: PumpEvents doesn't crash", "[platform][context]")
{
  TestServiceScope scope;

  auto ctx =
      PlatformContext::Create({.WindowBackend = WindowBackendKind::Null});
  ctx->PumpEvents();
}

TEST_CASE("Null backend: invalid window operations are safe",
          "[platform][context]")
{
  TestServiceScope scope;

  auto ctx =
      PlatformContext::Create({.WindowBackend = WindowBackendKind::Null});

  WindowHandle invalid;
  REQUIRE_FALSE(ctx->IsWindowAlive(invalid));
  REQUIRE_FALSE(ctx->RequestClose(invalid));
  ctx->DestroyWindow(invalid);
}
