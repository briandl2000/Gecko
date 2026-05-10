#pragma once

/// @file
/// State-based polled input service (keyboard, mouse, text input).
///
/// `IInput` is a per-frame state snapshot. The backend subscribes to
/// platform window events; calling `NewFrame()` rolls "this frame"
/// into "previous frame" so `WasKeyPressed` / `WasKeyReleased` can
/// report rising/falling edges. Apps that prefer push-style input can
/// subscribe to keyboard/mouse events on the event bus instead.

#include "gecko/core/api.h"
#include "gecko/core/types.h"
#include "gecko/platform/input_codes.h"
#include "gecko/platform/window.h"

#include <string_view>

namespace gecko::platform {

/// Window-relative mouse coordinates in pixels (origin top-left).
struct MousePosition
{
  i32 X {0};  ///< X coordinate in window pixels.
  i32 Y {0};  ///< Y coordinate in window pixels.
};

/// State-based input service. Snapshots per frame so callers can poll
/// instead of subscribing to events.
///
/// Coordinate semantics:
/// - `GetMousePosition()` returns the position within the focused window.
/// - `GetMousePosition(WindowHandle)` returns the last known position
///   within that specific window (not updated when the cursor leaves it).
/// - `GetMouseDelta()` is the change since the previous `NewFrame()`.
/// - `GetMouseScrollX/Y()` is the accumulated wheel delta since the
///   previous `NewFrame()` (cleared on `NewFrame`).
class IInput
{
public:
  virtual ~IInput() = default;

  /// Per-frame state advance. Snapshots current to previous and clears
  /// edge/scroll accumulators. Call exactly once per frame.
  GECKO_API virtual void NewFrame() noexcept = 0;

  // Keyboard --------------------------------------------------------
  /// `true` while `key` is held.
  [[nodiscard]] GECKO_API virtual bool IsKeyDown(KeyCode key) const noexcept = 0;
  /// `true` for the single frame on which `key` transitioned to down.
  [[nodiscard]] GECKO_API virtual bool WasKeyPressed(KeyCode key) const noexcept = 0;
  /// `true` for the single frame on which `key` transitioned to up.
  [[nodiscard]] GECKO_API virtual bool WasKeyReleased(KeyCode key) const noexcept = 0;

  // Mouse -----------------------------------------------------------
  /// `true` while `button` is held.
  [[nodiscard]] GECKO_API virtual bool IsMouseButtonDown(MouseButton button) const noexcept = 0;
  /// `true` for the single frame on which `button` transitioned to down.
  [[nodiscard]] GECKO_API virtual bool WasMouseButtonPressed(MouseButton button) const noexcept = 0;
  /// `true` for the single frame on which `button` transitioned to up.
  [[nodiscard]] GECKO_API virtual bool WasMouseButtonReleased(MouseButton button) const noexcept = 0;

  /// Cursor position within the currently focused window.
  [[nodiscard]] GECKO_API virtual MousePosition GetMousePosition() const noexcept = 0;
  /// Cursor position within `window` (last known value; not updated
  /// while the cursor is outside the window).
  [[nodiscard]] GECKO_API virtual MousePosition GetMousePosition(WindowHandle window) const noexcept = 0;
  /// Cursor movement since the previous `NewFrame()`.
  [[nodiscard]] GECKO_API virtual MousePosition GetMouseDelta() const noexcept = 0;
  /// Accumulated horizontal wheel delta since the previous `NewFrame()`.
  [[nodiscard]] GECKO_API virtual float GetMouseScrollX() const noexcept = 0;
  /// Accumulated vertical wheel delta since the previous `NewFrame()`.
  [[nodiscard]] GECKO_API virtual float GetMouseScrollY() const noexcept = 0;

  // Window focus / hover --------------------------------------------
  /// The window currently receiving keyboard focus, or
  /// `WindowHandle::InvalidId` when none of our windows is focused.
  [[nodiscard]] GECKO_API virtual WindowHandle FocusedWindow() const noexcept = 0;
  /// The window the mouse cursor is over. `WindowHandle::InvalidId` when
  /// the cursor is outside all of our windows or has not entered any
  /// since startup.
  [[nodiscard]] GECKO_API virtual WindowHandle HoveredWindow() const noexcept = 0;

  // Text input ------------------------------------------------------
  /// UTF-8 text typed since the last `NewFrame()`; cleared on `NewFrame()`.
  /// Reflects modifiers and IME composition (e.g. Shift+a -> "A",
  /// AltGr -> currency glyphs). The view is valid until the next
  /// `IInput` call; copy if you need to retain it across frames.
  [[nodiscard]] GECKO_API virtual ::std::string_view GetTypedText() const noexcept = 0;
};

/// Service accessor. Returns the published `IInput`, or `nullptr` when
/// `PlatformModule` has not started up (or has shut down).
[[nodiscard]] GECKO_API IInput* GetInput() noexcept;

// Convenience free functions -----------------------------------------
//
// All free functions below forward to `GetInput()` and return safe
// defaults when no input service is installed.

/// `IInput::IsKeyDown` via `GetInput()`. Returns `false` when no service.
[[nodiscard]] GECKO_API bool IsKeyDown(KeyCode key) noexcept;
/// `IInput::WasKeyPressed` via `GetInput()`. Returns `false` when no service.
[[nodiscard]] GECKO_API bool WasKeyPressed(KeyCode key) noexcept;
/// `IInput::WasKeyReleased` via `GetInput()`. Returns `false` when no service.
[[nodiscard]] GECKO_API bool WasKeyReleased(KeyCode key) noexcept;

/// `IInput::IsMouseButtonDown` via `GetInput()`.
[[nodiscard]] GECKO_API bool IsMouseButtonDown(MouseButton button) noexcept;
/// `IInput::WasMouseButtonPressed` via `GetInput()`.
[[nodiscard]] GECKO_API bool WasMouseButtonPressed(MouseButton button) noexcept;
/// `IInput::WasMouseButtonReleased` via `GetInput()`.
[[nodiscard]] GECKO_API bool WasMouseButtonReleased(MouseButton button) noexcept;

/// `IInput::GetMousePosition()` via `GetInput()`. Returns `{0,0}` when no
/// service.
[[nodiscard]] GECKO_API MousePosition GetMousePosition() noexcept;
/// `IInput::GetMousePosition(window)` via `GetInput()`.
[[nodiscard]] GECKO_API MousePosition GetMousePosition(WindowHandle window) noexcept;
/// `IInput::GetMouseDelta` via `GetInput()`. Returns `{0,0}` when no service.
[[nodiscard]] GECKO_API MousePosition GetMouseDelta() noexcept;
/// `IInput::GetMouseScrollX` via `GetInput()`. Returns `0` when no service.
[[nodiscard]] GECKO_API float GetMouseScrollX() noexcept;
/// `IInput::GetMouseScrollY` via `GetInput()`. Returns `0` when no service.
[[nodiscard]] GECKO_API float GetMouseScrollY() noexcept;

/// `IInput::FocusedWindow` via `GetInput()`.
[[nodiscard]] GECKO_API WindowHandle FocusedWindow() noexcept;
/// `IInput::HoveredWindow` via `GetInput()`.
[[nodiscard]] GECKO_API WindowHandle HoveredWindow() noexcept;

/// `IInput::GetTypedText` via `GetInput()`. Returns empty when no service.
[[nodiscard]] GECKO_API ::std::string_view GetTypedText() noexcept;

}  // namespace gecko::platform
