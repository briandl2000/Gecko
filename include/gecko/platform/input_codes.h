#pragma once

/// @file
/// Platform-agnostic key and mouse-button enumerations.
///
/// Values match Win32 Virtual-Key codes so the Win32 backend can pass
/// native codes through unchanged. X11, Wayland and Cocoa backends
/// translate their native codes into these. Letter and digit values
/// match ASCII for convenient direct comparison.

#include "gecko/core/types.h"

namespace gecko::platform {

/// Platform-agnostic keyboard key identifier. See `IInput` for usage.
enum class KeyCode : u16
{
  Unknown = 0x00,

  // Mouse buttons (VK_LBUTTON - VK_XBUTTON2)
  MouseLeft = 0x01,
  MouseRight = 0x02,
  MouseMiddle = 0x04,
  MouseX1 = 0x05,
  MouseX2 = 0x06,

  // Control keys
  Backspace = 0x08,
  Tab = 0x09,
  Clear = 0x0C,
  Enter = 0x0D,
  Shift = 0x10,
  Control = 0x11,
  Alt = 0x12,
  Pause = 0x13,
  CapsLock = 0x14,
  Escape = 0x1B,
  Space = 0x20,

  // Navigation
  PageUp = 0x21,
  PageDown = 0x22,
  End = 0x23,
  Home = 0x24,
  Left = 0x25,
  Up = 0x26,
  Right = 0x27,
  Down = 0x28,

  // Editing
  Insert = 0x2D,
  Delete = 0x2E,

  // Digits (match ASCII '0'-'9')
  D0 = 0x30,
  D1 = 0x31,
  D2 = 0x32,
  D3 = 0x33,
  D4 = 0x34,
  D5 = 0x35,
  D6 = 0x36,
  D7 = 0x37,
  D8 = 0x38,
  D9 = 0x39,

  // Letters (match ASCII 'A'-'Z')
  A = 0x41,
  B = 0x42,
  C = 0x43,
  D = 0x44,
  E = 0x45,
  F = 0x46,
  G = 0x47,
  H = 0x48,
  I = 0x49,
  J = 0x4A,
  K = 0x4B,
  L = 0x4C,
  M = 0x4D,
  N = 0x4E,
  O = 0x4F,
  P = 0x50,
  Q = 0x51,
  R = 0x52,
  S = 0x53,
  T = 0x54,
  U = 0x55,
  V = 0x56,
  W = 0x57,
  X = 0x58,
  Y = 0x59,
  Z = 0x5A,

  // Windows/Super keys
  LeftSuper = 0x5B,
  RightSuper = 0x5C,
  Menu = 0x5D,

  // Numpad
  Numpad0 = 0x60,
  Numpad1 = 0x61,
  Numpad2 = 0x62,
  Numpad3 = 0x63,
  Numpad4 = 0x64,
  Numpad5 = 0x65,
  Numpad6 = 0x66,
  Numpad7 = 0x67,
  Numpad8 = 0x68,
  Numpad9 = 0x69,
  NumpadMultiply = 0x6A,
  NumpadAdd = 0x6B,
  NumpadSeparator = 0x6C,
  NumpadSubtract = 0x6D,
  NumpadDecimal = 0x6E,
  NumpadDivide = 0x6F,

  // Function keys
  F1 = 0x70,
  F2 = 0x71,
  F3 = 0x72,
  F4 = 0x73,
  F5 = 0x74,
  F6 = 0x75,
  F7 = 0x76,
  F8 = 0x77,
  F9 = 0x78,
  F10 = 0x79,
  F11 = 0x7A,
  F12 = 0x7B,

  // Lock keys
  NumLock = 0x90,
  ScrollLock = 0x91,

  // Left/Right variants
  LeftShift = 0xA0,
  RightShift = 0xA1,
  LeftControl = 0xA2,
  RightControl = 0xA3,
  LeftAlt = 0xA4,
  RightAlt = 0xA5,

  // OEM keys (US standard layout)
  Semicolon = 0xBA,     // ;:
  Equal = 0xBB,         // =+
  Comma = 0xBC,         // ,<
  Minus = 0xBD,         // -_
  Period = 0xBE,        // .>
  Slash = 0xBF,         // /?
  GraveAccent = 0xC0,   // `~
  LeftBracket = 0xDB,   // [{
  Backslash = 0xDC,     // \|
  RightBracket = 0xDD,  // ]}
  Apostrophe = 0xDE,    // '"

  PrintScreen = 0x2C,
};

/// Mouse button identifier (matches the indices in `WindowMouseButtonPayload`).
enum class MouseButton : u8
{
  Left = 0,
  Right = 1,
  Middle = 2,
  X1 = 3,
  X2 = 4,
};

}  // namespace gecko::platform
