// Tests for the IInput service (NullInput + WindowEventInput).

#include "gecko/core/engine.h"
#include "gecko/core/services.h"
#include "gecko/core/services/events.h"
#include "gecko/platform/input.h"
#include "gecko/platform/platform_events.h"
#include "gecko/platform/platform_module.h"
#include "gecko/runtime/event_bus.h"
#include "gecko/runtime/runtime_module.h"

#include <catch2/catch_test_macros.hpp>
#include <optional>

using namespace gecko;
using namespace gecko::platform;

namespace {

// Lightweight scope: boots Engine with PlatformModule(Null backend) so
// the input service is published but no real OS windows are created.
struct InputTestScope
{
  SystemAllocator alloc;
  NullJobSystem jobs;
  NullProfiler profiler;
  NullLogger logger;
  runtime::EventBus events;
  runtime::CoreServicesModule runtimeMod;
  PlatformModule platformMod;
  ::std::optional<::gecko::Engine> engine;

  static PlatformConfig MakeNullConfig() noexcept
  {
    PlatformConfig cfg;
    cfg.Backend = DisplayBackendKind::Null;
    return cfg;
  }

  InputTestScope()
      : runtimeMod(jobs, profiler, logger, events),
        platformMod(MakeNullConfig())
  {
    REQUIRE(SetAllocator(&alloc));
    engine = ::gecko::Engine::Create({&runtimeMod, &platformMod});
    REQUIRE(engine.has_value());
  }

  ~InputTestScope()
  {
    engine.reset();
    ResetAllocator();
  }
};

// Helper: emit a WindowKey event and dispatch immediately.
void EmitKey(KeyCode key, bool down) noexcept
{
  auto emitter =
      ::gecko::CreateEmitterForModule(::gecko::platform::labels::Platform);
  events::WindowKeyPayload payload {.Window = WindowHandle {1},
                                    .TimeNs = 0,
                                    .Key = key,
                                    .Down = static_cast<u8>(down ? 1 : 0),
                                    .Repeat = 0};
  ::gecko::SendEvent(emitter, events::WindowKey, payload);
  (void)::gecko::DispatchEvents();
}

void EmitMouseMove(WindowHandle w, i32 x, i32 y) noexcept
{
  auto emitter =
      ::gecko::CreateEmitterForModule(::gecko::platform::labels::Platform);
  events::WindowMouseMovePayload payload {
      .Window = w, .TimeNs = 0, .X = x, .Y = y};
  ::gecko::SendEvent(emitter, events::WindowMouseMove, payload);
  (void)::gecko::DispatchEvents();
}

void EmitMouseButton(MouseButton b, bool down) noexcept
{
  auto emitter =
      ::gecko::CreateEmitterForModule(::gecko::platform::labels::Platform);
  events::WindowMouseButtonPayload payload {.Window = WindowHandle {1},
                                            .TimeNs = 0,
                                            .Button = b,
                                            .Down =
                                                static_cast<u8>(down ? 1 : 0)};
  ::gecko::SendEvent(emitter, events::WindowMouseButton, payload);
  (void)::gecko::DispatchEvents();
}

void EmitMouseWheel(float dx, float dy) noexcept
{
  auto emitter =
      ::gecko::CreateEmitterForModule(::gecko::platform::labels::Platform);
  events::WindowMouseWheelPayload payload {
      .Window = WindowHandle {1}, .TimeNs = 0, .DeltaX = dx, .DeltaY = dy};
  ::gecko::SendEvent(emitter, events::WindowMouseWheel, payload);
  (void)::gecko::DispatchEvents();
}

void EmitFocus(WindowHandle w, bool focused) noexcept
{
  auto emitter =
      ::gecko::CreateEmitterForModule(::gecko::platform::labels::Platform);
  events::WindowFocusChangedPayload payload {
      .Window = w, .TimeNs = 0, .Focused = static_cast<u8>(focused ? 1 : 0)};
  ::gecko::SendEvent(emitter, events::WindowFocusChanged, payload);
  (void)::gecko::DispatchEvents();
}

}  // namespace

TEST_CASE("Input service is published after PlatformModule startup",
          "[platform][input]")
{
  InputTestScope scope;
  REQUIRE(GetInput() != nullptr);
}

TEST_CASE("Input: key down state tracks events", "[platform][input]")
{
  InputTestScope scope;
  auto* in = GetInput();
  REQUIRE(in != nullptr);

  REQUIRE_FALSE(in->IsKeyDown(KeyCode::A));
  EmitKey(KeyCode::A, true);
  REQUIRE(in->IsKeyDown(KeyCode::A));
  EmitKey(KeyCode::A, false);
  REQUIRE_FALSE(in->IsKeyDown(KeyCode::A));
}

TEST_CASE("Input: WasKeyPressed is an edge", "[platform][input]")
{
  InputTestScope scope;
  auto* in = GetInput();

  in->NewFrame();  // baseline: A previously up
  EmitKey(KeyCode::A, true);
  REQUIRE(in->WasKeyPressed(KeyCode::A));

  in->NewFrame();  // now A is in prev as down
  REQUIRE_FALSE(in->WasKeyPressed(KeyCode::A));
  REQUIRE(in->IsKeyDown(KeyCode::A));
}

TEST_CASE("Input: WasKeyReleased is an edge", "[platform][input]")
{
  InputTestScope scope;
  auto* in = GetInput();

  EmitKey(KeyCode::B, true);
  in->NewFrame();
  EmitKey(KeyCode::B, false);
  REQUIRE(in->WasKeyReleased(KeyCode::B));
  in->NewFrame();
  REQUIRE_FALSE(in->WasKeyReleased(KeyCode::B));
}

TEST_CASE("Input: free-function helpers forward to service",
          "[platform][input]")
{
  InputTestScope scope;
  EmitKey(KeyCode::Space, true);
  REQUIRE(IsKeyDown(KeyCode::Space));
  EmitKey(KeyCode::Space, false);
  REQUIRE_FALSE(IsKeyDown(KeyCode::Space));
}

TEST_CASE("Input: mouse position updates on move", "[platform][input]")
{
  InputTestScope scope;
  auto* in = GetInput();

  WindowHandle w {42};
  EmitFocus(w, true);
  EmitMouseMove(w, 100, 200);
  auto pos = in->GetMousePosition();
  REQUIRE(pos.X == 100);
  REQUIRE(pos.Y == 200);
}

TEST_CASE("Input: mouse delta tracks across NewFrame", "[platform][input]")
{
  InputTestScope scope;
  auto* in = GetInput();

  WindowHandle w {1};
  EmitFocus(w, true);
  EmitMouseMove(w, 10, 20);
  in->NewFrame();
  EmitMouseMove(w, 30, 50);
  auto d = in->GetMouseDelta();
  REQUIRE(d.X == 20);
  REQUIRE(d.Y == 30);
}

TEST_CASE("Input: mouse button state + edge", "[platform][input]")
{
  InputTestScope scope;
  auto* in = GetInput();

  REQUIRE_FALSE(in->IsMouseButtonDown(MouseButton::Left));
  EmitMouseButton(MouseButton::Left, true);
  REQUIRE(in->IsMouseButtonDown(MouseButton::Left));
  REQUIRE(in->WasMouseButtonPressed(MouseButton::Left));
  in->NewFrame();
  REQUIRE_FALSE(in->WasMouseButtonPressed(MouseButton::Left));
  EmitMouseButton(MouseButton::Left, false);
  REQUIRE(in->WasMouseButtonReleased(MouseButton::Left));
}

TEST_CASE("Input: scroll accumulates and clears on NewFrame",
          "[platform][input]")
{
  InputTestScope scope;
  auto* in = GetInput();

  REQUIRE(in->GetMouseScrollY() == 0.0f);
  EmitMouseWheel(0.0f, 1.5f);
  EmitMouseWheel(0.0f, 2.0f);
  REQUIRE(in->GetMouseScrollY() == 3.5f);
  in->NewFrame();
  REQUIRE(in->GetMouseScrollY() == 0.0f);
}

TEST_CASE("Input: focus tracking", "[platform][input]")
{
  InputTestScope scope;
  auto* in = GetInput();

  REQUIRE_FALSE(in->FocusedWindow().IsValid());
  WindowHandle w {7};
  EmitFocus(w, true);
  REQUIRE(in->FocusedWindow() == w);
  EmitFocus(w, false);
  REQUIRE_FALSE(in->FocusedWindow().IsValid());
}
