#include "feature_platform_scope.h"
#include "gecko/platform/platform_events.h"
#include "gecko/platform/window.h"

#include <catch2/catch_test_macros.hpp>

using namespace gecko;
using namespace gecko::platform;

TEST_CASE("Live backend: create and destroy window",
          "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowDesc desc;
  desc.Title = "Feature Test Window";
  desc.Size = {320, 240};
  desc.Visible = false;  // Don't flash a window on screen

  WindowHandle win;
  REQUIRE(scope.Ctx.Windows().CreateWindow(desc, win));
  REQUIRE(win.IsValid());
  REQUIRE(scope.Ctx.Windows().IsWindowAlive(win));

  scope.Ctx.Windows().DestroyWindow(win);
  REQUIRE_FALSE(scope.Ctx.Windows().IsWindowAlive(win));
}

TEST_CASE("Live backend: client size matches requested size",
          "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowDesc desc;
  desc.Title = "Size Test";
  desc.Size = {800, 600};
  desc.Visible = false;

  WindowHandle win;
  REQUIRE(scope.Ctx.Windows().CreateWindow(desc, win));

  Extent2D size = scope.Ctx.Windows().GetClientSize(win);
  REQUIRE(size.Width == 800);
  REQUIRE(size.Height == 600);

  scope.Ctx.Windows().DestroyWindow(win);
}

TEST_CASE("Live backend: set title does not crash",
          "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win;
  scope.Ctx.Windows().CreateWindow({.Visible = false}, win);
  scope.Ctx.Windows().SetTitle(win, "New Title");
  scope.Ctx.Windows().DestroyWindow(win);
}

TEST_CASE("Live backend: multiple windows", "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowHandle w1, w2;
  REQUIRE(scope.Ctx.Windows().CreateWindow({.Visible = false}, w1));
  REQUIRE(scope.Ctx.Windows().CreateWindow({.Visible = false}, w2));
  REQUIRE(w1 != w2);
  REQUIRE(scope.Ctx.Windows().IsWindowAlive(w1));
  REQUIRE(scope.Ctx.Windows().IsWindowAlive(w2));

  scope.Ctx.Windows().DestroyWindow(w1);
  REQUIRE_FALSE(scope.Ctx.Windows().IsWindowAlive(w1));
  REQUIRE(scope.Ctx.Windows().IsWindowAlive(w2));

  scope.Ctx.Windows().DestroyWindow(w2);
}

TEST_CASE("Live backend: request close fires event",
          "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win;
  scope.Ctx.Windows().CreateWindow({.Visible = false}, win);
  REQUIRE(scope.Ctx.Windows().RequestClose(win));

  int received = 0;
  auto sub = SubscribeEvent(
      events::WindowCloseRequested,
      [](void* user, const EventMeta&, EventView) {
        (*static_cast<int*>(user))++;
      },
      &received);

  scope.Ctx.PumpEvents();
  (void)DispatchEvents();

  REQUIRE(received == 1);

  scope.Ctx.Windows().DestroyWindow(win);
}

TEST_CASE("Live backend: destroy fires WindowClosed event",
          "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win;
  scope.Ctx.Windows().CreateWindow({.Visible = false}, win);

  int received = 0;
  auto sub = SubscribeEvent(
      events::WindowClosed,
      [](void* user, const EventMeta&, EventView) {
        (*static_cast<int*>(user))++;
      },
      &received);

  scope.Ctx.Windows().DestroyWindow(win);
  scope.Ctx.PumpEvents();
  (void)DispatchEvents();

  REQUIRE(received == 1);
}

TEST_CASE("Live backend: native handle is populated",
          "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win;
  scope.Ctx.Windows().CreateWindow({.Visible = false}, win);

  NativeWindowHandle nh = scope.Ctx.Windows().GetNativeWindowHandle(win);

  // The backend may fall back to Null when the window backend for the
  // resolved display backend is not yet implemented (e.g. Wayland).
  // We verify the handle is at least consistently set.
  REQUIRE(nh.Backend != DisplayBackendKind::Auto);

  scope.Ctx.Windows().DestroyWindow(win);
}

TEST_CASE("Live backend: pump events does not crash",
          "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win;
  scope.Ctx.Windows().CreateWindow({.Visible = false}, win);

  for (int i = 0; i < 10; ++i)
    scope.Ctx.PumpEvents();

  scope.Ctx.Windows().DestroyWindow(win);
}
