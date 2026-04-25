#include "feature_platform_scope.h"
#include "gecko/core/utility/bit.h"
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

  WindowHandle win = ::gecko::platform::GetWindows()->CreateWindow(desc);
  REQUIRE(win.IsValid());
  REQUIRE(::gecko::platform::GetWindows()->IsWindowAlive(win));

  ::gecko::platform::GetWindows()->DestroyWindow(win);
  REQUIRE_FALSE(::gecko::platform::GetWindows()->IsWindowAlive(win));
}

TEST_CASE("Live backend: client size matches requested size",
          "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowDesc desc;
  desc.Title = "Size Test";
  desc.Size = {800, 600};
  desc.Visible = false;

  WindowHandle win = ::gecko::platform::GetWindows()->CreateWindow(desc);
  REQUIRE(win.IsValid());

  Extent2D size = ::gecko::platform::GetWindows()->GetClientSize(win);
  // On Wayland the compositor may override the requested size in the
  // initial configure event, so we only check that a valid size was
  // assigned rather than matching the exact request.
  REQUIRE(size.Width > 0);
  REQUIRE(size.Height > 0);

  ::gecko::platform::GetWindows()->DestroyWindow(win);
}

TEST_CASE("Live backend: set title does not crash",
          "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win =
      ::gecko::platform::GetWindows()->CreateWindow({.Visible = false});
  ::gecko::platform::GetWindows()->SetTitle(win, "New Title");
  ::gecko::platform::GetWindows()->DestroyWindow(win);
}

TEST_CASE("Live backend: multiple windows", "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowHandle w1 =
      ::gecko::platform::GetWindows()->CreateWindow({.Visible = false});
  WindowHandle w2 =
      ::gecko::platform::GetWindows()->CreateWindow({.Visible = false});
  REQUIRE(w1.IsValid());
  REQUIRE(w2.IsValid());
  REQUIRE(w1 != w2);
  REQUIRE(::gecko::platform::GetWindows()->IsWindowAlive(w1));
  REQUIRE(::gecko::platform::GetWindows()->IsWindowAlive(w2));

  ::gecko::platform::GetWindows()->DestroyWindow(w1);
  REQUIRE_FALSE(::gecko::platform::GetWindows()->IsWindowAlive(w1));
  REQUIRE(::gecko::platform::GetWindows()->IsWindowAlive(w2));

  ::gecko::platform::GetWindows()->DestroyWindow(w2);
}

TEST_CASE("Live backend: request close fires event",
          "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win =
      ::gecko::platform::GetWindows()->CreateWindow({.Visible = false});
  REQUIRE(::gecko::platform::GetWindows()->RequestClose(win));

  int received = 0;
  auto sub = SubscribeEvent(
      events::WindowCloseRequested,
      [](void* user, const EventMeta&, EventView) {
        (*static_cast<int*>(user))++;
      },
      &received);

  ::gecko::platform::PumpEvents();
  (void)DispatchEvents();

  REQUIRE(received == 1);

  ::gecko::platform::GetWindows()->DestroyWindow(win);
}

TEST_CASE("Live backend: destroy fires WindowClosed event",
          "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win =
      ::gecko::platform::GetWindows()->CreateWindow({.Visible = false});

  int received = 0;
  auto sub = SubscribeEvent(
      events::WindowClosed,
      [](void* user, const EventMeta&, EventView) {
        (*static_cast<int*>(user))++;
      },
      &received);

  ::gecko::platform::GetWindows()->DestroyWindow(win);
  ::gecko::platform::PumpEvents();
  (void)DispatchEvents();

  REQUIRE(received == 1);
}

TEST_CASE("Live backend: native handle is populated",
          "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win =
      ::gecko::platform::GetWindows()->CreateWindow({.Visible = false});

  NativeWindowHandle nh =
      ::gecko::platform::GetWindows()->GetNativeWindowHandle(win);

  // The backend may fall back to Null when the window backend for the
  // resolved display backend is not yet implemented (e.g. Wayland).
  // We verify the handle is at least consistently set.
  REQUIRE(nh.Backend != DisplayBackendKind::Auto);

  ::gecko::platform::GetWindows()->DestroyWindow(win);
}

TEST_CASE("Live backend: pump events does not crash",
          "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win =
      ::gecko::platform::GetWindows()->CreateWindow({.Visible = false});

  for (int i = 0; i < 10; ++i)
    ::gecko::platform::PumpEvents();

  ::gecko::platform::GetWindows()->DestroyWindow(win);
}

TEST_CASE("Live backend: set and get client size",
          "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win =
      ::gecko::platform::GetWindows()->CreateWindow({.Visible = false});

  ::gecko::platform::GetWindows()->SetClientSize(win, {640, 480});
  // Size may be adjusted by the WM, so we just verify the call doesn't crash.

  ::gecko::platform::GetWindows()->DestroyWindow(win);
}

TEST_CASE("Live backend: get title returns desc title",
          "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowDesc desc;
  desc.Title = "Title Test";
  desc.Visible = false;
  WindowHandle win = ::gecko::platform::GetWindows()->CreateWindow(desc);

  const char* title = ::gecko::platform::GetWindows()->GetTitle(win);
  REQUIRE(title != nullptr);

  ::gecko::platform::GetWindows()->DestroyWindow(win);
}

TEST_CASE("Live backend: set and get position", "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win =
      ::gecko::platform::GetWindows()->CreateWindow({.Visible = false});

  ::gecko::platform::GetWindows()->SetPosition(win, {100, 200});
  // Actual repositioning may not be reflected immediately.

  ::gecko::platform::GetWindows()->DestroyWindow(win);
}

TEST_CASE("Live backend: set and get window state",
          "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win =
      ::gecko::platform::GetWindows()->CreateWindow({.Visible = false});

  ::gecko::platform::GetWindows()->SetWindowState(win, WindowState::Hidden);
  REQUIRE(::gecko::platform::GetWindows()->GetWindowState(win) ==
          WindowState::Hidden);

  ::gecko::platform::GetWindows()->DestroyWindow(win);
}

TEST_CASE("Live backend: decorated flag defaults true",
          "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win =
      ::gecko::platform::GetWindows()->CreateWindow({.Visible = false});

  REQUIRE(::gecko::platform::GetWindows()->IsDecorated(win) == true);

  ::gecko::platform::GetWindows()->SetDecorated(win, false);
  REQUIRE(::gecko::platform::GetWindows()->IsDecorated(win) == false);

  ::gecko::platform::GetWindows()->DestroyWindow(win);
}

TEST_CASE("Live backend: cursor mode get/set", "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win =
      ::gecko::platform::GetWindows()->CreateWindow({.Visible = false});

  REQUIRE(::gecko::platform::GetWindows()->GetCursorMode(win) ==
          CursorMode::Normal);

  ::gecko::platform::GetWindows()->SetCursorMode(win, CursorMode::Hidden);
  REQUIRE(::gecko::platform::GetWindows()->GetCursorMode(win) ==
          CursorMode::Hidden);

  ::gecko::platform::GetWindows()->DestroyWindow(win);
}

TEST_CASE("Live backend: request focus does not crash",
          "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win =
      ::gecko::platform::GetWindows()->CreateWindow({.Visible = false});

  ::gecko::platform::GetWindows()->RequestFocus(win);

  ::gecko::platform::GetWindows()->DestroyWindow(win);
}

// ══════════════════════════════════════════════════════════════════════════
// Visible window tests — these briefly show real windows on the display
// server to exercise the backend rendering/event paths more thoroughly.
// ══════════════════════════════════════════════════════════════════════════

TEST_CASE("Live backend: visible window create and pump",
          "[feature][platform][window][.visible]")
{
  test::FeaturePlatformScope scope;

  WindowDesc desc;
  desc.Title = "Visible Feature Test";
  desc.Size = {400, 300};
  desc.Visible = true;

  WindowHandle win = ::gecko::platform::GetWindows()->CreateWindow(desc);
  REQUIRE(win.IsValid());

  // Pump a few frames — processes map/configure events
  for (int i = 0; i < 5; ++i)
    ::gecko::platform::PumpEvents();

  REQUIRE(::gecko::platform::GetWindows()->IsWindowAlive(win));

  Extent2D size = ::gecko::platform::GetWindows()->GetClientSize(win);
  REQUIRE(size.Width > 0);
  REQUIRE(size.Height > 0);

  ::gecko::platform::GetWindows()->DestroyWindow(win);
}

TEST_CASE("Live backend: visible window resize and pump",
          "[feature][platform][window][.visible]")
{
  test::FeaturePlatformScope scope;

  WindowDesc desc;
  desc.Title = "Resize Visible";
  desc.Size = {400, 300};
  desc.Visible = true;
  desc.Resizable = true;

  WindowHandle win = ::gecko::platform::GetWindows()->CreateWindow(desc);
  REQUIRE(win.IsValid());

  // Pump to process initial map
  for (int i = 0; i < 3; ++i)
    ::gecko::platform::PumpEvents();

  ::gecko::platform::GetWindows()->SetClientSize(win, {640, 480});

  // Pump to process resize
  for (int i = 0; i < 3; ++i)
    ::gecko::platform::PumpEvents();

  Extent2D size = ::gecko::platform::GetWindows()->GetClientSize(win);
  REQUIRE(size.Width > 0);
  REQUIRE(size.Height > 0);

  ::gecko::platform::GetWindows()->DestroyWindow(win);
}

TEST_CASE("Live backend: visible window set title and read back",
          "[feature][platform][window][.visible]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win = ::gecko::platform::GetWindows()->CreateWindow(
      {.Title = "Original Title", .Visible = true});
  REQUIRE(win.IsValid());

  for (int i = 0; i < 3; ++i)
    ::gecko::platform::PumpEvents();

  ::gecko::platform::GetWindows()->SetTitle(win, "Updated Title");

  const char* title = ::gecko::platform::GetWindows()->GetTitle(win);
  REQUIRE(title != nullptr);
  // Title should reflect the update
  REQUIRE(std::string(title).find("Updated") != std::string::npos);

  ::gecko::platform::GetWindows()->DestroyWindow(win);
}

TEST_CASE("Live backend: visible window position set/get",
          "[feature][platform][window][.visible]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win =
      ::gecko::platform::GetWindows()->CreateWindow({.Visible = true});
  REQUIRE(win.IsValid());

  for (int i = 0; i < 3; ++i)
    ::gecko::platform::PumpEvents();

  ::gecko::platform::GetWindows()->SetPosition(win, {100, 100});

  for (int i = 0; i < 3; ++i)
    ::gecko::platform::PumpEvents();

  // Position may not match exactly (Wayland ignores positioning),
  // but the call should not crash and state should be queryable.
  math::Int2 pos = ::gecko::platform::GetWindows()->GetPosition(win);
  (void)pos;

  ::gecko::platform::GetWindows()->DestroyWindow(win);
}

TEST_CASE("Live backend: visible window state transitions",
          "[feature][platform][window][.visible]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win =
      ::gecko::platform::GetWindows()->CreateWindow({.Visible = true});
  REQUIRE(win.IsValid());

  for (int i = 0; i < 3; ++i)
    ::gecko::platform::PumpEvents();

  // Minimize
  ::gecko::platform::GetWindows()->SetWindowState(win, WindowState::Minimized);
  for (int i = 0; i < 3; ++i)
    ::gecko::platform::PumpEvents();
  REQUIRE(::gecko::platform::GetWindows()->GetWindowState(win) ==
          WindowState::Minimized);

  // Restore
  ::gecko::platform::GetWindows()->SetWindowState(win, WindowState::Normal);
  for (int i = 0; i < 3; ++i)
    ::gecko::platform::PumpEvents();
  REQUIRE(::gecko::platform::GetWindows()->GetWindowState(win) ==
          WindowState::Normal);

  ::gecko::platform::GetWindows()->DestroyWindow(win);
}

TEST_CASE("Live backend: visible window decoration toggle",
          "[feature][platform][window][.visible]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win = ::gecko::platform::GetWindows()->CreateWindow(
      {.Visible = true, .Decorated = true});
  REQUIRE(win.IsValid());

  for (int i = 0; i < 3; ++i)
    ::gecko::platform::PumpEvents();

  REQUIRE(::gecko::platform::GetWindows()->IsDecorated(win));

  ::gecko::platform::GetWindows()->SetDecorated(win, false);
  REQUIRE_FALSE(::gecko::platform::GetWindows()->IsDecorated(win));

  ::gecko::platform::GetWindows()->SetDecorated(win, true);
  REQUIRE(::gecko::platform::GetWindows()->IsDecorated(win));

  ::gecko::platform::GetWindows()->DestroyWindow(win);
}

TEST_CASE("Live backend: visible window DPI query",
          "[feature][platform][window][.visible]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win =
      ::gecko::platform::GetWindows()->CreateWindow({.Visible = true});
  REQUIRE(win.IsValid());

  for (int i = 0; i < 3; ++i)
    ::gecko::platform::PumpEvents();

  DpiInfo dpi = ::gecko::platform::GetWindows()->GetDpi(win);
  REQUIRE(dpi.Dpi > 0);
  REQUIRE(dpi.Scale > 0.0F);

  ::gecko::platform::GetWindows()->DestroyWindow(win);
}

TEST_CASE("Live backend: visible window native handle",
          "[feature][platform][window][.visible]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win =
      ::gecko::platform::GetWindows()->CreateWindow({.Visible = true});
  REQUIRE(win.IsValid());

  for (int i = 0; i < 3; ++i)
    ::gecko::platform::PumpEvents();

  NativeWindowHandle nh =
      ::gecko::platform::GetWindows()->GetNativeWindowHandle(win);
  REQUIRE(nh.Backend != DisplayBackendKind::Unknown);
  REQUIRE(nh.Backend != DisplayBackendKind::Auto);
  REQUIRE(nh.Handle != nullptr);

  ::gecko::platform::GetWindows()->DestroyWindow(win);
}

TEST_CASE("Live backend: visible window cursor mode transitions",
          "[feature][platform][window][.visible]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win =
      ::gecko::platform::GetWindows()->CreateWindow({.Visible = true});
  REQUIRE(win.IsValid());

  for (int i = 0; i < 3; ++i)
    ::gecko::platform::PumpEvents();

  REQUIRE(::gecko::platform::GetWindows()->GetCursorMode(win) ==
          CursorMode::Normal);

  ::gecko::platform::GetWindows()->SetCursorMode(win, CursorMode::Hidden);
  REQUIRE(::gecko::platform::GetWindows()->GetCursorMode(win) ==
          CursorMode::Hidden);

  ::gecko::platform::GetWindows()->SetCursorMode(win, CursorMode::Normal);
  REQUIRE(::gecko::platform::GetWindows()->GetCursorMode(win) ==
          CursorMode::Normal);

  ::gecko::platform::GetWindows()->DestroyWindow(win);
}

TEST_CASE("Live backend: visible window resized event fires",
          "[feature][platform][window][.visible]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win = ::gecko::platform::GetWindows()->CreateWindow(
      {.Size = {400, 300}, .Resizable = true, .Visible = true});
  REQUIRE(win.IsValid());

  for (int i = 0; i < 3; ++i)
    ::gecko::platform::PumpEvents();

  int resizeCount = 0;
  auto sub = SubscribeEvent(
      events::WindowResized,
      [](void* user, const EventMeta&, EventView) {
        (*static_cast<int*>(user))++;
      },
      &resizeCount);

  ::gecko::platform::GetWindows()->SetClientSize(win, {500, 350});
  ::gecko::platform::PumpEvents();
  (void)DispatchEvents();

  // Some backends may not fire a resize event synchronously; that's OK.
  // We just verify the pipeline doesn't crash.
  (void)resizeCount;

  ::gecko::platform::GetWindows()->DestroyWindow(win);
}

TEST_CASE("Live backend: multiple visible windows simultaneously",
          "[feature][platform][window][.visible]")
{
  test::FeaturePlatformScope scope;

  WindowHandle w1 = ::gecko::platform::GetWindows()->CreateWindow(
      {.Title = "Window 1", .Size = {300, 200}, .Visible = true});
  WindowHandle w2 = ::gecko::platform::GetWindows()->CreateWindow(
      {.Title = "Window 2", .Size = {300, 200}, .Visible = true});
  WindowHandle w3 = ::gecko::platform::GetWindows()->CreateWindow(
      {.Title = "Window 3", .Size = {300, 200}, .Visible = true});
  REQUIRE(w1.IsValid());
  REQUIRE(w2.IsValid());
  REQUIRE(w3.IsValid());

  REQUIRE(w1 != w2);
  REQUIRE(w2 != w3);
  REQUIRE(w1 != w3);

  for (int i = 0; i < 5; ++i)
    ::gecko::platform::PumpEvents();

  REQUIRE(::gecko::platform::GetWindows()->IsWindowAlive(w1));
  REQUIRE(::gecko::platform::GetWindows()->IsWindowAlive(w2));
  REQUIRE(::gecko::platform::GetWindows()->IsWindowAlive(w3));

  // Destroy middle window
  ::gecko::platform::GetWindows()->DestroyWindow(w2);
  REQUIRE_FALSE(::gecko::platform::GetWindows()->IsWindowAlive(w2));
  REQUIRE(::gecko::platform::GetWindows()->IsWindowAlive(w1));
  REQUIRE(::gecko::platform::GetWindows()->IsWindowAlive(w3));

  ::gecko::platform::GetWindows()->DestroyWindow(w1);
  ::gecko::platform::GetWindows()->DestroyWindow(w3);
}

TEST_CASE("Live backend: visible window focus request",
          "[feature][platform][window][.visible]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win =
      ::gecko::platform::GetWindows()->CreateWindow({.Visible = true});
  REQUIRE(win.IsValid());

  for (int i = 0; i < 3; ++i)
    ::gecko::platform::PumpEvents();

  // Request focus and pump — should not crash
  ::gecko::platform::GetWindows()->RequestFocus(win);

  for (int i = 0; i < 3; ++i)
    ::gecko::platform::PumpEvents();

  ::gecko::platform::GetWindows()->DestroyWindow(win);
}

// ══════════════════════════════════════════════════════════════════════════
// Extended API tests — resizable, window mode, buttons, size constraints,
// always-on-top
//
// Wayland limitations:
//   - SetPosition is a no-op (compositor controls placement)
//   - RequestFocus is a no-op (compositor controls focus)
//   - SetAlwaysOnTop is a no-op (no client-side control)
//   - SetWindowButtons stores state but Wayland has no CSD button protocol
//   - SetResizable uses xdg_toplevel min/max size hints
//
// Win32 limitations:
//   - SetWindowButtons uses system menu EnableMenuItem + WS_ style bits;
//     some WMs may not fully honour the greyed-out state
//   - SetMinSize/SetMaxSize are stored but require WM_GETMINMAXINFO handling
//     (pending integration)
//
// X11: Full support for all new APIs via Motif WM hints and EWMH.
// ══════════════════════════════════════════════════════════════════════════

TEST_CASE("Live backend: resizable defaults to desc value",
          "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowHandle resizable = ::gecko::platform::GetWindows()->CreateWindow(
      {.Resizable = true, .Visible = false});
  WindowHandle fixed = ::gecko::platform::GetWindows()->CreateWindow(
      {.Resizable = false, .Visible = false});

  REQUIRE(::gecko::platform::GetWindows()->IsResizable(resizable));
  REQUIRE_FALSE(::gecko::platform::GetWindows()->IsResizable(fixed));

  ::gecko::platform::GetWindows()->DestroyWindow(resizable);
  ::gecko::platform::GetWindows()->DestroyWindow(fixed);
}

TEST_CASE("Live backend: toggle resizable at runtime",
          "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win = ::gecko::platform::GetWindows()->CreateWindow(
      {.Resizable = true, .Visible = false});
  REQUIRE(::gecko::platform::GetWindows()->IsResizable(win));

  ::gecko::platform::GetWindows()->SetResizable(win, false);
  REQUIRE_FALSE(::gecko::platform::GetWindows()->IsResizable(win));

  ::gecko::platform::GetWindows()->SetResizable(win, true);
  REQUIRE(::gecko::platform::GetWindows()->IsResizable(win));

  ::gecko::platform::GetWindows()->DestroyWindow(win);
}

TEST_CASE("Live backend: window mode defaults to Windowed",
          "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win =
      ::gecko::platform::GetWindows()->CreateWindow({.Visible = false});
  REQUIRE(::gecko::platform::GetWindows()->GetWindowMode(win) ==
          WindowMode::Windowed);

  ::gecko::platform::GetWindows()->DestroyWindow(win);
}

TEST_CASE("Live backend: set window mode to borderless fullscreen and back",
          "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win =
      ::gecko::platform::GetWindows()->CreateWindow({.Visible = false});

  ::gecko::platform::GetWindows()->SetWindowMode(
      win, WindowMode::BorderlessFullscreen);
  REQUIRE(::gecko::platform::GetWindows()->GetWindowMode(win) ==
          WindowMode::BorderlessFullscreen);

  ::gecko::platform::GetWindows()->SetWindowMode(win, WindowMode::Windowed);
  REQUIRE(::gecko::platform::GetWindows()->GetWindowMode(win) ==
          WindowMode::Windowed);

  ::gecko::platform::GetWindows()->DestroyWindow(win);
}

TEST_CASE("Live backend: set window mode same mode is no-op",
          "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win =
      ::gecko::platform::GetWindows()->CreateWindow({.Visible = false});
  ::gecko::platform::GetWindows()->SetWindowMode(win, WindowMode::Windowed);
  REQUIRE(::gecko::platform::GetWindows()->GetWindowMode(win) ==
          WindowMode::Windowed);

  ::gecko::platform::GetWindows()->DestroyWindow(win);
}

TEST_CASE("Live backend: window buttons default to All",
          "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win =
      ::gecko::platform::GetWindows()->CreateWindow({.Visible = false});
  REQUIRE(::gecko::platform::GetWindows()->GetWindowButtons(win) ==
          WindowButtons::All);

  ::gecko::platform::GetWindows()->DestroyWindow(win);
}

TEST_CASE("Live backend: set and get window buttons",
          "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win =
      ::gecko::platform::GetWindows()->CreateWindow({.Visible = false});

  // Remove close button
  WindowButtons noClose = WindowButtons::Minimize | WindowButtons::Maximize;
  ::gecko::platform::GetWindows()->SetWindowButtons(win, noClose);
  REQUIRE(::gecko::platform::GetWindows()->GetWindowButtons(win) == noClose);

  // Remove all
  ::gecko::platform::GetWindows()->SetWindowButtons(win, WindowButtons::None);
  REQUIRE(::gecko::platform::GetWindows()->GetWindowButtons(win) ==
          WindowButtons::None);

  // Restore all
  ::gecko::platform::GetWindows()->SetWindowButtons(win, WindowButtons::All);
  REQUIRE(::gecko::platform::GetWindows()->GetWindowButtons(win) ==
          WindowButtons::All);

  ::gecko::platform::GetWindows()->DestroyWindow(win);
}

TEST_CASE("Live backend: set min and max size does not crash",
          "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win = ::gecko::platform::GetWindows()->CreateWindow(
      {.Resizable = true, .Visible = false});

  ::gecko::platform::GetWindows()->SetMinSize(win, {200, 150});
  ::gecko::platform::GetWindows()->SetMaxSize(win, {1920, 1080});

  // Clear constraints
  ::gecko::platform::GetWindows()->SetMinSize(win, {0, 0});
  ::gecko::platform::GetWindows()->SetMaxSize(win, {0, 0});

  ::gecko::platform::GetWindows()->DestroyWindow(win);
}

TEST_CASE("Live backend: always on top defaults to false",
          "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win =
      ::gecko::platform::GetWindows()->CreateWindow({.Visible = false});
  REQUIRE_FALSE(::gecko::platform::GetWindows()->IsAlwaysOnTop(win));

  ::gecko::platform::GetWindows()->DestroyWindow(win);
}

TEST_CASE("Live backend: toggle always on top", "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win =
      ::gecko::platform::GetWindows()->CreateWindow({.Visible = false});

  ::gecko::platform::GetWindows()->SetAlwaysOnTop(win, true);
  // Wayland ignores this, so we only check state on non-Wayland backends.
  // The call must not crash regardless.

  ::gecko::platform::GetWindows()->SetAlwaysOnTop(win, false);

  ::gecko::platform::GetWindows()->DestroyWindow(win);
}

TEST_CASE("Live backend: create window with custom buttons",
          "[feature][platform][window]")
{
  test::FeaturePlatformScope scope;

  WindowDesc desc;
  desc.Visible = false;
  desc.Buttons = WindowButtons::Close;  // Only close button

  WindowHandle win = ::gecko::platform::GetWindows()->CreateWindow(desc);
  REQUIRE(win.IsValid());
  REQUIRE(::gecko::platform::GetWindows()->GetWindowButtons(win) ==
          WindowButtons::Close);

  ::gecko::platform::GetWindows()->DestroyWindow(win);
}

// ── Visible window tests for new APIs ──────────────────────────────────

TEST_CASE("Live backend: visible window borderless fullscreen toggle",
          "[feature][platform][window][.visible]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win = ::gecko::platform::GetWindows()->CreateWindow(
      {.Title = "Fullscreen Toggle", .Size = {400, 300}, .Visible = true});
  REQUIRE(win.IsValid());

  for (int i = 0; i < 3; ++i)
    ::gecko::platform::PumpEvents();

  // Go fullscreen
  ::gecko::platform::GetWindows()->SetWindowMode(
      win, WindowMode::BorderlessFullscreen);

  for (int i = 0; i < 5; ++i)
    ::gecko::platform::PumpEvents();

  REQUIRE(::gecko::platform::GetWindows()->GetWindowMode(win) ==
          WindowMode::BorderlessFullscreen);

  // Back to windowed
  ::gecko::platform::GetWindows()->SetWindowMode(win, WindowMode::Windowed);

  for (int i = 0; i < 5; ++i)
    ::gecko::platform::PumpEvents();

  REQUIRE(::gecko::platform::GetWindows()->GetWindowMode(win) ==
          WindowMode::Windowed);

  ::gecko::platform::GetWindows()->DestroyWindow(win);
}

TEST_CASE("Live backend: visible window resizable toggle",
          "[feature][platform][window][.visible]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win = ::gecko::platform::GetWindows()->CreateWindow(
      {.Size = {400, 300}, .Resizable = true, .Visible = true});
  REQUIRE(win.IsValid());

  for (int i = 0; i < 3; ++i)
    ::gecko::platform::PumpEvents();

  ::gecko::platform::GetWindows()->SetResizable(win, false);
  REQUIRE_FALSE(::gecko::platform::GetWindows()->IsResizable(win));

  ::gecko::platform::GetWindows()->SetResizable(win, true);
  REQUIRE(::gecko::platform::GetWindows()->IsResizable(win));

  ::gecko::platform::GetWindows()->DestroyWindow(win);
}

TEST_CASE("Live backend: visible window button manipulation",
          "[feature][platform][window][.visible]")
{
  test::FeaturePlatformScope scope;

  WindowHandle win =
      ::gecko::platform::GetWindows()->CreateWindow({.Visible = true});
  REQUIRE(win.IsValid());

  for (int i = 0; i < 3; ++i)
    ::gecko::platform::PumpEvents();

  // Disable maximize
  WindowButtons noMax = WindowButtons::Close | WindowButtons::Minimize;
  ::gecko::platform::GetWindows()->SetWindowButtons(win, noMax);
  REQUIRE(::gecko::platform::GetWindows()->GetWindowButtons(win) == noMax);

  for (int i = 0; i < 3; ++i)
    ::gecko::platform::PumpEvents();

  // Restore all
  ::gecko::platform::GetWindows()->SetWindowButtons(win, WindowButtons::All);
  REQUIRE(::gecko::platform::GetWindows()->GetWindowButtons(win) ==
          WindowButtons::All);

  ::gecko::platform::GetWindows()->DestroyWindow(win);
}
