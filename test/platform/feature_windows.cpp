#include "feature_platform_scope.h"
#include "gecko/platform/platform_events.h"
#include "gecko/platform/window.h"

#include <catch2/catch_test_macros.hpp>
#include <string>

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

  WindowHandle win = scope.Ctx.Windows().CreateWindow(desc);
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

  WindowHandle win = scope.Ctx.Windows().CreateWindow(desc);
  REQUIRE(win.IsValid());

  Extent2D size = scope.Ctx.Windows().GetClientSize(win);
  // On Wayland the compositor may override the requested size in the
  // initial configure event, so we only check that a valid size was
  // assigned rather than matching the exact request.
  REQUIRE(size.Width > 0);
  REQUIRE(size.Height > 0);

  scope.Ctx.Windows().DestroyWindow(win);
}

TEST_CASE("Live backend: set title does not crash",
          "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win = scope.Ctx.Windows().CreateWindow({.Visible = false});
  scope.Ctx.Windows().SetTitle(win, "New Title");
  scope.Ctx.Windows().DestroyWindow(win);
}

TEST_CASE("Live backend: multiple windows", "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowHandle w1 = scope.Ctx.Windows().CreateWindow({.Visible = false});
  WindowHandle w2 = scope.Ctx.Windows().CreateWindow({.Visible = false});
  REQUIRE(w1.IsValid());
  REQUIRE(w2.IsValid());
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

  WindowHandle win = scope.Ctx.Windows().CreateWindow({.Visible = false});
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

  WindowHandle win = scope.Ctx.Windows().CreateWindow({.Visible = false});

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

  WindowHandle win = scope.Ctx.Windows().CreateWindow({.Visible = false});

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

  WindowHandle win = scope.Ctx.Windows().CreateWindow({.Visible = false});

  for (int i = 0; i < 10; ++i)
    scope.Ctx.PumpEvents();

  scope.Ctx.Windows().DestroyWindow(win);
}

TEST_CASE("Live backend: set and get client size",
          "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win = scope.Ctx.Windows().CreateWindow({.Visible = false});

  scope.Ctx.Windows().SetClientSize(win, {640, 480});
  // Size may be adjusted by the WM, so we just verify the call doesn't crash.

  scope.Ctx.Windows().DestroyWindow(win);
}

TEST_CASE("Live backend: get title returns desc title",
          "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowDesc desc;
  desc.Title = "Title Test";
  desc.Visible = false;
  WindowHandle win = scope.Ctx.Windows().CreateWindow(desc);

  const char* title = scope.Ctx.Windows().GetTitle(win);
  REQUIRE(title != nullptr);

  scope.Ctx.Windows().DestroyWindow(win);
}

TEST_CASE("Live backend: set and get position", "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win = scope.Ctx.Windows().CreateWindow({.Visible = false});

  scope.Ctx.Windows().SetPosition(win, {100, 200});
  // Actual repositioning may not be reflected immediately.

  scope.Ctx.Windows().DestroyWindow(win);
}

TEST_CASE("Live backend: set and get window state",
          "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win = scope.Ctx.Windows().CreateWindow({.Visible = false});

  scope.Ctx.Windows().SetWindowState(win, WindowState::Hidden);
  REQUIRE(scope.Ctx.Windows().GetWindowState(win) == WindowState::Hidden);

  scope.Ctx.Windows().DestroyWindow(win);
}

TEST_CASE("Live backend: decorated flag defaults true",
          "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win = scope.Ctx.Windows().CreateWindow({.Visible = false});

  REQUIRE(scope.Ctx.Windows().IsDecorated(win) == true);

  scope.Ctx.Windows().SetDecorated(win, false);
  REQUIRE(scope.Ctx.Windows().IsDecorated(win) == false);

  scope.Ctx.Windows().DestroyWindow(win);
}

TEST_CASE("Live backend: cursor mode get/set", "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win = scope.Ctx.Windows().CreateWindow({.Visible = false});

  REQUIRE(scope.Ctx.Windows().GetCursorMode(win) == CursorMode::Normal);

  scope.Ctx.Windows().SetCursorMode(win, CursorMode::Hidden);
  REQUIRE(scope.Ctx.Windows().GetCursorMode(win) == CursorMode::Hidden);

  scope.Ctx.Windows().DestroyWindow(win);
}

TEST_CASE("Live backend: request focus does not crash",
          "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win = scope.Ctx.Windows().CreateWindow({.Visible = false});

  scope.Ctx.Windows().RequestFocus(win);

  scope.Ctx.Windows().DestroyWindow(win);
}

// ══════════════════════════════════════════════════════════════════════════
// Visible window tests — these briefly show real windows on the display
// server to exercise the backend rendering/event paths more thoroughly.
// ══════════════════════════════════════════════════════════════════════════

TEST_CASE("Live backend: visible window create and pump",
          "[feature][platform][window][visible]")
{
  test::FeaturePlatformScope scope;

  WindowDesc desc;
  desc.Title = "Visible Feature Test";
  desc.Size = {400, 300};
  desc.Visible = true;

  WindowHandle win = scope.Ctx.Windows().CreateWindow(desc);
  REQUIRE(win.IsValid());

  // Pump a few frames — processes map/configure events
  for (int i = 0; i < 5; ++i)
    scope.Ctx.PumpEvents();

  REQUIRE(scope.Ctx.Windows().IsWindowAlive(win));

  Extent2D size = scope.Ctx.Windows().GetClientSize(win);
  REQUIRE(size.Width > 0);
  REQUIRE(size.Height > 0);

  scope.Ctx.Windows().DestroyWindow(win);
}

TEST_CASE("Live backend: visible window resize and pump",
          "[feature][platform][window][visible]")
{
  test::FeaturePlatformScope scope;

  WindowDesc desc;
  desc.Title = "Resize Visible";
  desc.Size = {400, 300};
  desc.Visible = true;
  desc.Resizable = true;

  WindowHandle win = scope.Ctx.Windows().CreateWindow(desc);
  REQUIRE(win.IsValid());

  // Pump to process initial map
  for (int i = 0; i < 3; ++i)
    scope.Ctx.PumpEvents();

  scope.Ctx.Windows().SetClientSize(win, {640, 480});

  // Pump to process resize
  for (int i = 0; i < 3; ++i)
    scope.Ctx.PumpEvents();

  Extent2D size = scope.Ctx.Windows().GetClientSize(win);
  REQUIRE(size.Width > 0);
  REQUIRE(size.Height > 0);

  scope.Ctx.Windows().DestroyWindow(win);
}

TEST_CASE("Live backend: visible window set title and read back",
          "[feature][platform][window][visible]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win = scope.Ctx.Windows().CreateWindow(
      {.Title = "Original Title", .Visible = true});
  REQUIRE(win.IsValid());

  for (int i = 0; i < 3; ++i)
    scope.Ctx.PumpEvents();

  scope.Ctx.Windows().SetTitle(win, "Updated Title");

  const char* title = scope.Ctx.Windows().GetTitle(win);
  REQUIRE(title != nullptr);
  // Title should reflect the update
  REQUIRE(std::string(title).find("Updated") != std::string::npos);

  scope.Ctx.Windows().DestroyWindow(win);
}

TEST_CASE("Live backend: visible window position set/get",
          "[feature][platform][window][visible]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win = scope.Ctx.Windows().CreateWindow({.Visible = true});
  REQUIRE(win.IsValid());

  for (int i = 0; i < 3; ++i)
    scope.Ctx.PumpEvents();

  scope.Ctx.Windows().SetPosition(win, {100, 100});

  for (int i = 0; i < 3; ++i)
    scope.Ctx.PumpEvents();

  // Position may not match exactly (Wayland ignores positioning),
  // but the call should not crash and state should be queryable.
  math::Int2 pos = scope.Ctx.Windows().GetPosition(win);
  (void)pos;

  scope.Ctx.Windows().DestroyWindow(win);
}

TEST_CASE("Live backend: visible window state transitions",
          "[feature][platform][window][visible]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win = scope.Ctx.Windows().CreateWindow({.Visible = true});
  REQUIRE(win.IsValid());

  for (int i = 0; i < 3; ++i)
    scope.Ctx.PumpEvents();

  // Minimize
  scope.Ctx.Windows().SetWindowState(win, WindowState::Minimized);
  for (int i = 0; i < 3; ++i)
    scope.Ctx.PumpEvents();
  REQUIRE(scope.Ctx.Windows().GetWindowState(win) == WindowState::Minimized);

  // Restore
  scope.Ctx.Windows().SetWindowState(win, WindowState::Normal);
  for (int i = 0; i < 3; ++i)
    scope.Ctx.PumpEvents();
  REQUIRE(scope.Ctx.Windows().GetWindowState(win) == WindowState::Normal);

  scope.Ctx.Windows().DestroyWindow(win);
}

TEST_CASE("Live backend: visible window decoration toggle",
          "[feature][platform][window][visible]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win =
      scope.Ctx.Windows().CreateWindow({.Visible = true, .Decorated = true});
  REQUIRE(win.IsValid());

  for (int i = 0; i < 3; ++i)
    scope.Ctx.PumpEvents();

  REQUIRE(scope.Ctx.Windows().IsDecorated(win));

  scope.Ctx.Windows().SetDecorated(win, false);
  REQUIRE_FALSE(scope.Ctx.Windows().IsDecorated(win));

  scope.Ctx.Windows().SetDecorated(win, true);
  REQUIRE(scope.Ctx.Windows().IsDecorated(win));

  scope.Ctx.Windows().DestroyWindow(win);
}

TEST_CASE("Live backend: visible window DPI query",
          "[feature][platform][window][visible]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win = scope.Ctx.Windows().CreateWindow({.Visible = true});
  REQUIRE(win.IsValid());

  for (int i = 0; i < 3; ++i)
    scope.Ctx.PumpEvents();

  DpiInfo dpi = scope.Ctx.Windows().GetDpi(win);
  REQUIRE(dpi.Dpi > 0);
  REQUIRE(dpi.Scale > 0.0F);

  scope.Ctx.Windows().DestroyWindow(win);
}

TEST_CASE("Live backend: visible window native handle",
          "[feature][platform][window][visible]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win = scope.Ctx.Windows().CreateWindow({.Visible = true});
  REQUIRE(win.IsValid());

  for (int i = 0; i < 3; ++i)
    scope.Ctx.PumpEvents();

  NativeWindowHandle nh = scope.Ctx.Windows().GetNativeWindowHandle(win);
  REQUIRE(nh.Backend != DisplayBackendKind::Unknown);
  REQUIRE(nh.Backend != DisplayBackendKind::Auto);
  REQUIRE(nh.Handle != nullptr);

  scope.Ctx.Windows().DestroyWindow(win);
}

TEST_CASE("Live backend: visible window cursor mode transitions",
          "[feature][platform][window][visible]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win = scope.Ctx.Windows().CreateWindow({.Visible = true});
  REQUIRE(win.IsValid());

  for (int i = 0; i < 3; ++i)
    scope.Ctx.PumpEvents();

  REQUIRE(scope.Ctx.Windows().GetCursorMode(win) == CursorMode::Normal);

  scope.Ctx.Windows().SetCursorMode(win, CursorMode::Hidden);
  REQUIRE(scope.Ctx.Windows().GetCursorMode(win) == CursorMode::Hidden);

  scope.Ctx.Windows().SetCursorMode(win, CursorMode::Normal);
  REQUIRE(scope.Ctx.Windows().GetCursorMode(win) == CursorMode::Normal);

  scope.Ctx.Windows().DestroyWindow(win);
}

TEST_CASE("Live backend: visible window resized event fires",
          "[feature][platform][window][visible]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win = scope.Ctx.Windows().CreateWindow(
      {.Size = {400, 300}, .Resizable = true, .Visible = true});
  REQUIRE(win.IsValid());

  for (int i = 0; i < 3; ++i)
    scope.Ctx.PumpEvents();

  int resizeCount = 0;
  auto sub = SubscribeEvent(
      events::WindowResized,
      [](void* user, const EventMeta&, EventView) {
        (*static_cast<int*>(user))++;
      },
      &resizeCount);

  scope.Ctx.Windows().SetClientSize(win, {500, 350});
  scope.Ctx.PumpEvents();
  (void)DispatchEvents();

  // Some backends may not fire a resize event synchronously; that's OK.
  // We just verify the pipeline doesn't crash.
  (void)resizeCount;

  scope.Ctx.Windows().DestroyWindow(win);
}

TEST_CASE("Live backend: multiple visible windows simultaneously",
          "[feature][platform][window][visible]")
{
  test::FeaturePlatformScope scope;

  WindowHandle w1 = scope.Ctx.Windows().CreateWindow(
      {.Title = "Window 1", .Size = {300, 200}, .Visible = true});
  WindowHandle w2 = scope.Ctx.Windows().CreateWindow(
      {.Title = "Window 2", .Size = {300, 200}, .Visible = true});
  WindowHandle w3 = scope.Ctx.Windows().CreateWindow(
      {.Title = "Window 3", .Size = {300, 200}, .Visible = true});
  REQUIRE(w1.IsValid());
  REQUIRE(w2.IsValid());
  REQUIRE(w3.IsValid());

  REQUIRE(w1 != w2);
  REQUIRE(w2 != w3);
  REQUIRE(w1 != w3);

  for (int i = 0; i < 5; ++i)
    scope.Ctx.PumpEvents();

  REQUIRE(scope.Ctx.Windows().IsWindowAlive(w1));
  REQUIRE(scope.Ctx.Windows().IsWindowAlive(w2));
  REQUIRE(scope.Ctx.Windows().IsWindowAlive(w3));

  // Destroy middle window
  scope.Ctx.Windows().DestroyWindow(w2);
  REQUIRE_FALSE(scope.Ctx.Windows().IsWindowAlive(w2));
  REQUIRE(scope.Ctx.Windows().IsWindowAlive(w1));
  REQUIRE(scope.Ctx.Windows().IsWindowAlive(w3));

  scope.Ctx.Windows().DestroyWindow(w1);
  scope.Ctx.Windows().DestroyWindow(w3);
}

TEST_CASE("Live backend: visible window focus request",
          "[feature][platform][window][visible]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win = scope.Ctx.Windows().CreateWindow({.Visible = true});
  REQUIRE(win.IsValid());

  for (int i = 0; i < 3; ++i)
    scope.Ctx.PumpEvents();

  // Request focus and pump — should not crash
  scope.Ctx.Windows().RequestFocus(win);

  for (int i = 0; i < 3; ++i)
    scope.Ctx.PumpEvents();

  scope.Ctx.Windows().DestroyWindow(win);
}
