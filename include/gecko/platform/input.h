#pragma once

#include "gecko/core/api.h"
#include "gecko/core/types.h"
#include "gecko/platform/input_codes.h"
#include "gecko/platform/window.h"

namespace gecko::platform {

// Window-relative mouse coordinates (origin top-left), in pixels.
struct MousePosition
{
  i32 X {0};
  i32 Y {0};
};

// State-based input service. Holds per-frame snapshots so the rest of
// the app can poll instead of subscribing to events.
//
// Lifecycle: backend subscribes to platform window events at
// construction, mutates internal state as events arrive, and NewFrame()
// rolls "this frame" into "previous frame" so WasKeyPressed() /
// WasKeyReleased() can report edges.
//
// Coordinate semantics:
//   - GetMousePosition() returns position within the focused window.
//   - GetMousePosition(WindowHandle) returns position within that
//     specific window (last known value; not updated when the cursor
//     leaves the window).
//   - GetMouseDelta() is the change since the previous NewFrame().
//   - GetMouseScroll{X,Y}() is the accumulated wheel delta since the
//     previous NewFrame() (cleared on NewFrame).
class IInput
{
public:
  virtual ~IInput() = default;

  // Per-frame state advance. Snapshots current → previous, clears
  // edge/scroll accumulators. App must call once per frame (Engine
  // tick / explicit call).
  GECKO_API virtual void NewFrame() noexcept = 0;

  // ── Keyboard ──────────────────────────────────────────────────
  [[nodiscard]] GECKO_API virtual bool IsKeyDown(
      KeyCode key) const noexcept = 0;
  [[nodiscard]] GECKO_API virtual bool WasKeyPressed(
      KeyCode key) const noexcept = 0;
  [[nodiscard]] GECKO_API virtual bool WasKeyReleased(
      KeyCode key) const noexcept = 0;

  // ── Mouse ─────────────────────────────────────────────────────
  [[nodiscard]] GECKO_API virtual bool IsMouseButtonDown(
      MouseButton button) const noexcept = 0;
  [[nodiscard]] GECKO_API virtual bool WasMouseButtonPressed(
      MouseButton button) const noexcept = 0;
  [[nodiscard]] GECKO_API virtual bool WasMouseButtonReleased(
      MouseButton button) const noexcept = 0;

  [[nodiscard]] GECKO_API virtual MousePosition GetMousePosition()
      const noexcept = 0;
  [[nodiscard]] GECKO_API virtual MousePosition GetMousePosition(
      WindowHandle window) const noexcept = 0;
  [[nodiscard]] GECKO_API virtual MousePosition GetMouseDelta()
      const noexcept = 0;
  [[nodiscard]] GECKO_API virtual float GetMouseScrollX() const noexcept = 0;
  [[nodiscard]] GECKO_API virtual float GetMouseScrollY() const noexcept = 0;

  // ── Window focus / hover ──────────────────────────────────────
  [[nodiscard]] GECKO_API virtual WindowHandle FocusedWindow()
      const noexcept = 0;
  // The window the mouse cursor is currently over (across any of our
  // windows). InvalidWindowHandle when the cursor is outside all of
  // them or has not entered any since startup.
  [[nodiscard]] GECKO_API virtual WindowHandle HoveredWindow()
      const noexcept = 0;
};

// Service accessor — available between PlatformModule::Startup and
// Shutdown. Returns nullptr otherwise.
[[nodiscard]] GECKO_API IInput* GetInput() noexcept;

// ── Convenience free functions ────────────────────────────────────
//
// All forward to GetInput(); return defaults when no input service is
// installed (NullInput is the fallback during boot).

[[nodiscard]] GECKO_API bool IsKeyDown(KeyCode key) noexcept;
[[nodiscard]] GECKO_API bool WasKeyPressed(KeyCode key) noexcept;
[[nodiscard]] GECKO_API bool WasKeyReleased(KeyCode key) noexcept;

[[nodiscard]] GECKO_API bool IsMouseButtonDown(MouseButton button) noexcept;
[[nodiscard]] GECKO_API bool WasMouseButtonPressed(MouseButton button) noexcept;
[[nodiscard]] GECKO_API bool WasMouseButtonReleased(
    MouseButton button) noexcept;

[[nodiscard]] GECKO_API MousePosition GetMousePosition() noexcept;
[[nodiscard]] GECKO_API MousePosition GetMouseDelta() noexcept;
[[nodiscard]] GECKO_API float GetMouseScrollY() noexcept;

[[nodiscard]] GECKO_API WindowHandle FocusedWindow() noexcept;
[[nodiscard]] GECKO_API WindowHandle HoveredWindow() noexcept;

}  // namespace gecko::platform
