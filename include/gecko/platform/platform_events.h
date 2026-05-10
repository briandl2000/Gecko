#pragma once

/// @file
/// Event codes and payload structs for window and monitor events.
///
/// All platform events are keyed to the `gecko.platform` module so the
/// event bus can validate emitter/code consistency at runtime.
/// Event-code layout:
/// - Window events:  local `0x0001 .. 0x00FF`.
/// - Monitor events: local `0x0100 .. 0x01FF`.
///
/// Payloads are flat, trivially copyable structs (no unions). In a
/// subscriber, cast `EventView::Data()` to the type whose name matches
/// the event code (e.g. `WindowClosed` -> `WindowClosedPayload`).

#include "gecko/core/labels.h"
#include "gecko/core/services/events.h"
#include "gecko/core/types.h"
#include "gecko/platform/input_codes.h"
#include "gecko/platform/monitor.h"
#include "gecko/platform/window.h"

namespace gecko::platform::events {

namespace detail {
/// Module id every platform event code is namespaced under.
inline constexpr u64 PlatformModuleId = ::gecko::MakeLabel("gecko.platform").Id;
}  // namespace detail

// Window event codes -------------------------------------------------

/// Emitted after a window has been destroyed. The `Window` handle is no
/// longer valid for new operations; the event is the last reference.
inline constexpr gecko::EventCode WindowClosed = gecko::MakeEventCode(detail::PlatformModuleId, 0x0001);
/// Emitted when the user requests the window to close (X button, Alt-F4).
/// The window is still alive; subscribers may veto by ignoring the event
/// or schedule cleanup before destroying it.
inline constexpr gecko::EventCode WindowCloseRequested = gecko::MakeEventCode(detail::PlatformModuleId, 0x0002);
/// Emitted whenever the client area dimensions change.
inline constexpr gecko::EventCode WindowResized = gecko::MakeEventCode(detail::PlatformModuleId, 0x0003);
/// Emitted when the per-window DPI / content scale changes.
inline constexpr gecko::EventCode WindowDpiChanged = gecko::MakeEventCode(detail::PlatformModuleId, 0x0004);
/// Emitted on key press, release, and OS-driven auto-repeat.
inline constexpr gecko::EventCode WindowKey = gecko::MakeEventCode(detail::PlatformModuleId, 0x0005);
/// Emitted for translated text input (one Unicode codepoint per event).
inline constexpr gecko::EventCode WindowChar = gecko::MakeEventCode(detail::PlatformModuleId, 0x0006);
/// Emitted when the mouse moves over the window's client area.
inline constexpr gecko::EventCode WindowMouseMove = gecko::MakeEventCode(detail::PlatformModuleId, 0x0007);
/// Emitted on mouse-button press and release.
inline constexpr gecko::EventCode WindowMouseButton = gecko::MakeEventCode(detail::PlatformModuleId, 0x0008);
/// Emitted on scroll-wheel motion (vertical and horizontal).
inline constexpr gecko::EventCode WindowMouseWheel = gecko::MakeEventCode(detail::PlatformModuleId, 0x0009);
/// Emitted when the window gains or loses keyboard focus.
inline constexpr gecko::EventCode WindowFocusChanged = gecko::MakeEventCode(detail::PlatformModuleId, 0x000A);
/// Emitted when the window's top-left position changes.
inline constexpr gecko::EventCode WindowMoved = gecko::MakeEventCode(detail::PlatformModuleId, 0x000B);
/// Emitted when `WindowState` transitions (Normal/Minimized/Maximized/...).
inline constexpr gecko::EventCode WindowStateChanged = gecko::MakeEventCode(detail::PlatformModuleId, 0x000C);
/// Emitted when the cursor enters the window's client area.
inline constexpr gecko::EventCode WindowMouseEntered = gecko::MakeEventCode(detail::PlatformModuleId, 0x000D);
/// Emitted when the cursor leaves the window's client area.
inline constexpr gecko::EventCode WindowMouseExited = gecko::MakeEventCode(detail::PlatformModuleId, 0x000E);

// Monitor event codes ------------------------------------------------

/// Emitted when a new monitor is attached to the system.
inline constexpr gecko::EventCode MonitorConnected = gecko::MakeEventCode(detail::PlatformModuleId, 0x0100);
/// Emitted when a monitor is detached. The handle is no longer valid.
inline constexpr gecko::EventCode MonitorDisconnected = gecko::MakeEventCode(detail::PlatformModuleId, 0x0101);
/// Emitted when an existing monitor's properties change (resolution,
/// refresh rate, position, primary flag, ...).
inline constexpr gecko::EventCode MonitorReconfigured = gecko::MakeEventCode(detail::PlatformModuleId, 0x0102);

// Window event payloads ----------------------------------------------

/// Payload for `WindowClosed`.
struct WindowClosedPayload
{
  WindowHandle Window;  ///< Window that was destroyed (no longer usable).
  u64 TimeNs;           ///< Event timestamp in nanoseconds.
};

/// Payload for `WindowCloseRequested`.
struct WindowCloseRequestedPayload
{
  WindowHandle Window;  ///< Window the close was requested for.
  u64 TimeNs;           ///< Event timestamp in nanoseconds.
};

/// Payload for `WindowResized`.
struct WindowResizedPayload
{
  WindowHandle Window;  ///< Window that resized.
  u64 TimeNs;           ///< Event timestamp in nanoseconds.
  u32 Width;            ///< New client-area width in pixels.
  u32 Height;           ///< New client-area height in pixels.
};

/// Payload for `WindowDpiChanged`.
struct WindowDpiChangedPayload
{
  WindowHandle Window;  ///< Window whose DPI changed.
  u64 TimeNs;           ///< Event timestamp in nanoseconds.
  u32 Dpi;              ///< New DPI in dots-per-inch (96 = 1.0 scale).
  float Scale;          ///< New content scale factor (`Dpi / 96.0f`).
};

/// Payload for `WindowKey`.
struct WindowKeyPayload
{
  WindowHandle Window;  ///< Window holding keyboard focus.
  u64 TimeNs;           ///< Event timestamp in nanoseconds.
  KeyCode Key;          ///< Logical key that changed state.
  u8 Down;              ///< `1` on press, `0` on release.
  u8 Repeat;            ///< `1` if generated by OS auto-repeat.
};

/// Payload for `WindowChar`.
struct WindowCharPayload
{
  WindowHandle Window;  ///< Window holding keyboard focus.
  u64 TimeNs;           ///< Event timestamp in nanoseconds.
  u32 Codepoint;        ///< Unicode codepoint produced by the OS IME / layout.
};

/// Payload for `WindowMouseMove`.
struct WindowMouseMovePayload
{
  WindowHandle Window;  ///< Window the cursor is over.
  u64 TimeNs;           ///< Event timestamp in nanoseconds.
  i32 X;                ///< X position in client-area pixels.
  i32 Y;                ///< Y position in client-area pixels (origin top-left).
};

/// Payload for `WindowMouseButton`.
struct WindowMouseButtonPayload
{
  WindowHandle Window;  ///< Window receiving the click.
  u64 TimeNs;           ///< Event timestamp in nanoseconds.
  MouseButton Button;   ///< Button that changed state.
  u8 Down;              ///< `1` on press, `0` on release.
};

/// Payload for `WindowMouseWheel`.
struct WindowMouseWheelPayload
{
  WindowHandle Window;  ///< Window the cursor is over.
  u64 TimeNs;           ///< Event timestamp in nanoseconds.
  float DeltaX;         ///< Horizontal scroll delta (one notch == 1.0).
  float DeltaY;         ///< Vertical scroll delta (one notch == 1.0).
};

/// Payload for `WindowFocusChanged`.
struct WindowFocusChangedPayload
{
  WindowHandle Window;  ///< Window whose focus state changed.
  u64 TimeNs;           ///< Event timestamp in nanoseconds.
  u8 Focused;           ///< `1` if the window now has focus, `0` if it lost it.
};

/// Payload for `WindowMoved`.
struct WindowMovedPayload
{
  WindowHandle Window;  ///< Window that moved.
  u64 TimeNs;           ///< Event timestamp in nanoseconds.
  i32 X;                ///< New top-left X in virtual-desktop pixels.
  i32 Y;                ///< New top-left Y in virtual-desktop pixels.
};

/// Payload for `WindowStateChanged`.
struct WindowStateChangedPayload
{
  WindowHandle Window;   ///< Window whose state changed.
  u64 TimeNs;            ///< Event timestamp in nanoseconds.
  WindowState OldState;  ///< Previous window state.
  WindowState NewState;  ///< New window state.
};

/// Payload for `WindowMouseEntered`.
struct WindowMouseEnteredPayload
{
  WindowHandle Window;  ///< Window the cursor entered.
  u64 TimeNs;           ///< Event timestamp in nanoseconds.
};

/// Payload for `WindowMouseExited`.
struct WindowMouseExitedPayload
{
  WindowHandle Window;  ///< Window the cursor left.
  u64 TimeNs;           ///< Event timestamp in nanoseconds.
};

// Monitor event payloads ---------------------------------------------

/// Payload for `MonitorConnected`.
struct MonitorConnectedPayload
{
  MonitorHandle Monitor;  ///< Newly connected monitor.
  u64 TimeNs;             ///< Event timestamp in nanoseconds.
  MonitorInfo Info;       ///< Snapshot of the monitor's properties.
};

/// Payload for `MonitorDisconnected`.
struct MonitorDisconnectedPayload
{
  MonitorHandle Monitor;  ///< Handle of the removed monitor (no longer valid).
  u64 TimeNs;             ///< Event timestamp in nanoseconds.
};

/// Payload for `MonitorReconfigured`.
struct MonitorReconfiguredPayload
{
  MonitorHandle Monitor;      ///< Monitor whose properties changed.
  u64 TimeNs;                 ///< Event timestamp in nanoseconds.
  MonitorInfo NewProperties;  ///< Snapshot of the updated properties.
};

}  // namespace gecko::platform::events
