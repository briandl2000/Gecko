// Live-backend smoke tests for the IInput service. Only checks that
// the service is wired up against the real platform backend and that
// queries don't crash. Actual OS-level input synthesis (XWarpPointer,
// SendInput, ...) is deferred — feature tests are run headlessly and
// real input injection is too platform-specific to be worth the
// flakiness here.

#include "feature_platform_scope.h"
#include "gecko/platform/input.h"
#include "gecko/platform/windows_interface.h"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <thread>

using namespace gecko;
using namespace gecko::platform;

TEST_CASE("Live backend: IInput service is published", "[feature][platform][input]")
{
  if (!::gecko::test::HasLiveDisplay())
    SKIP("No live display available");

  ::gecko::test::FeaturePlatformScope scope;
  REQUIRE(GetInput() != nullptr);
}

TEST_CASE("Live backend: IInput initial state is empty", "[feature][platform][input]")
{
  if (!::gecko::test::HasLiveDisplay())
    SKIP("No live display available");

  ::gecko::test::FeaturePlatformScope scope;
  auto* in = GetInput();
  REQUIRE(in != nullptr);
  REQUIRE_FALSE(in->IsKeyDown(KeyCode::A));
  REQUIRE_FALSE(in->IsMouseButtonDown(MouseButton::Left));
  REQUIRE(in->GetMouseScrollX() == 0.0f);
  REQUIRE(in->GetMouseScrollY() == 0.0f);
}

TEST_CASE("Live backend: PumpEvents auto-rolls input edges without crashing", "[feature][platform][input]")
{
  if (!::gecko::test::HasLiveDisplay())
    SKIP("No live display available");

  ::gecko::test::FeaturePlatformScope scope;
  for (int i = 0; i < 5; ++i)
  {
    PumpEvents();
    (void)::gecko::DispatchEvents();
  }
  // Just making sure the integration is stable across multiple frames.
  SUCCEED("PumpEvents+DispatchEvents loop completed");
}

TEST_CASE("Live backend: IInput accessors safe to query with no input", "[feature][platform][input]")
{
  if (!::gecko::test::HasLiveDisplay())
    SKIP("No live display available");

  ::gecko::test::FeaturePlatformScope scope;
  auto* in = GetInput();
  REQUIRE(in != nullptr);

  // No crashes / no UB regardless of OS input state.
  (void)in->FocusedWindow();
  (void)in->HoveredWindow();
  (void)in->GetMousePosition();
  (void)in->GetMouseDelta();
  (void)in->WasKeyPressed(KeyCode::Space);
  (void)in->WasMouseButtonReleased(MouseButton::Right);
  SUCCEED();
}

// ── Visible / interactive showcase ────────────────────────────────
//
// Tagged [.visible] so it's hidden from the default test run. Run with:
//
//   gk test debug --visible
//
// (or the binary directly: platform_feature_tests "[.visible]")
//
// Opens a real window, pumps for ~3 seconds so the user can actually
// see the backend doing its thing, then asserts the IInput service
// stayed coherent across the loop (no service drop-out, no crash on
// repeated NewFrame via PumpEvents).
TEST_CASE("Live backend: visible IInput showcase", "[.visible][feature][platform][input][window]")
{
  ::gecko::test::FeaturePlatformScope scope;

  WindowDesc desc;
  desc.Title = "Gecko IInput showcase — move/click/scroll, then wait";
  desc.Size = {640, 360};
  desc.Visible = true;

  WindowHandle win = GetWindows()->CreateWindow(desc);
  REQUIRE(win.IsValid());
  REQUIRE(GetInput() != nullptr);

  // Pump for ~3 seconds so the window actually renders and the user
  // can see + interact with it. Sleep keeps CPU low without blocking
  // the event loop; PumpEvents itself drives the IInput frame model.
  using clock = ::std::chrono::steady_clock;
  const auto deadline = clock::now() + ::std::chrono::seconds(3);
  while (clock::now() < deadline)
  {
    PumpEvents();
    (void)::gecko::DispatchEvents();
    REQUIRE(GetInput() != nullptr);
    ::std::this_thread::sleep_for(::std::chrono::milliseconds(16));
  }

  REQUIRE(GetWindows()->IsWindowAlive(win));
  GetWindows()->DestroyWindow(win);
}

// Showcase for IInput::GetTypedText. Opens a window for ~5 s and logs
// whatever the user types. Always passes — the goal is a visible demo
// that the OS → events::WindowChar → IInput::GetTypedText pipeline is
// alive on the current backend.
TEST_CASE("Live backend: visible IInput typed-text showcase", "[.visible][feature][platform][input][text][window]")
{
  ::gecko::test::FeaturePlatformScope scope;

  WindowDesc desc;
  desc.Title = "Gecko text-input showcase \xE2\x80\x94 type something";
  desc.Size = {640, 200};
  desc.Visible = true;

  WindowHandle win = GetWindows()->CreateWindow(desc);
  REQUIRE(win.IsValid());

  auto* in = GetInput();
  REQUIRE(in != nullptr);

  ::std::string accumulated;
  using clock = ::std::chrono::steady_clock;
  const auto deadline = clock::now() + ::std::chrono::seconds(5);
  while (clock::now() < deadline)
  {
    PumpEvents();
    (void)::gecko::DispatchEvents();
    accumulated.append(in->GetTypedText());
    ::std::this_thread::sleep_for(::std::chrono::milliseconds(16));
  }

  // No assertion on content — depends on user. Just keep it visible
  // in the output so the developer can eyeball the result.
  WARN("accumulated typed text: \"" << accumulated << "\"");

  GetWindows()->DestroyWindow(win);
}
