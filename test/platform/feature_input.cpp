// Live-backend smoke tests for the IInput service. Only checks that
// the service is wired up against the real platform backend and that
// queries don't crash. Actual OS-level input synthesis (XWarpPointer,
// SendInput, ...) is deferred — feature tests are run headlessly and
// real input injection is too platform-specific to be worth the
// flakiness here.

#include "feature_platform_scope.h"
#include "gecko/platform/input.h"

#include <catch2/catch_test_macros.hpp>

using namespace gecko;
using namespace gecko::platform;

TEST_CASE("Live backend: IInput service is published",
          "[feature][platform][input]")
{
  if (!::gecko::test::HasLiveDisplay())
    SKIP("No live display available");

  ::gecko::test::FeaturePlatformScope scope;
  REQUIRE(GetInput() != nullptr);
}

TEST_CASE("Live backend: IInput initial state is empty",
          "[feature][platform][input]")
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

TEST_CASE("Live backend: PumpEvents auto-rolls input edges without crashing",
          "[feature][platform][input]")
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

TEST_CASE("Live backend: IInput accessors safe to query with no input",
          "[feature][platform][input]")
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
