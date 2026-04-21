#pragma once

#include "gecko/platform/input_codes.h"

#include <X11/keysym.h>

namespace gecko::platform {

inline KeyCode X11KeySymToKeyCode(unsigned long keysym) noexcept
{
  // Letters (XK_a–XK_z and XK_A–XK_Z → KeyCode::A–Z)
  if (keysym >= XK_a && keysym <= XK_z)
    return static_cast<KeyCode>(0x41 + (keysym - XK_a));
  if (keysym >= XK_A && keysym <= XK_Z)
    return static_cast<KeyCode>(0x41 + (keysym - XK_A));

  // Digits (XK_0–XK_9 → KeyCode::D0–D9)
  if (keysym >= XK_0 && keysym <= XK_9)
    return static_cast<KeyCode>(0x30 + (keysym - XK_0));

  // Numpad digits (XK_KP_0–XK_KP_9 → KeyCode::Numpad0–Numpad9)
  if (keysym >= XK_KP_0 && keysym <= XK_KP_9)
    return static_cast<KeyCode>(0x60 + (keysym - XK_KP_0));

  // Function keys (XK_F1–XK_F12 → KeyCode::F1–F12)
  if (keysym >= XK_F1 && keysym <= XK_F12)
    return static_cast<KeyCode>(0x70 + (keysym - XK_F1));

  switch (keysym)
  {
  case XK_BackSpace:
    return KeyCode::Backspace;
  case XK_Tab:
    return KeyCode::Tab;
  case XK_Return:
  case XK_KP_Enter:
    return KeyCode::Enter;
  case XK_Escape:
    return KeyCode::Escape;
  case XK_space:
    return KeyCode::Space;

  // Navigation
  case XK_Page_Up:
    return KeyCode::PageUp;
  case XK_Page_Down:
    return KeyCode::PageDown;
  case XK_End:
    return KeyCode::End;
  case XK_Home:
    return KeyCode::Home;
  case XK_Left:
    return KeyCode::Left;
  case XK_Up:
    return KeyCode::Up;
  case XK_Right:
    return KeyCode::Right;
  case XK_Down:
    return KeyCode::Down;

  // Editing
  case XK_Insert:
    return KeyCode::Insert;
  case XK_Delete:
    return KeyCode::Delete;

  // Modifiers
  case XK_Shift_L:
    return KeyCode::LeftShift;
  case XK_Shift_R:
    return KeyCode::RightShift;
  case XK_Control_L:
    return KeyCode::LeftControl;
  case XK_Control_R:
    return KeyCode::RightControl;
  case XK_Alt_L:
    return KeyCode::LeftAlt;
  case XK_Alt_R:
    return KeyCode::RightAlt;
  case XK_Super_L:
    return KeyCode::LeftSuper;
  case XK_Super_R:
    return KeyCode::RightSuper;
  case XK_Menu:
    return KeyCode::Menu;
  case XK_Caps_Lock:
    return KeyCode::CapsLock;
  case XK_Num_Lock:
    return KeyCode::NumLock;
  case XK_Scroll_Lock:
    return KeyCode::ScrollLock;
  case XK_Pause:
    return KeyCode::Pause;
  case XK_Print:
    return KeyCode::PrintScreen;

  // Numpad operators
  case XK_KP_Multiply:
    return KeyCode::NumpadMultiply;
  case XK_KP_Add:
    return KeyCode::NumpadAdd;
  case XK_KP_Subtract:
    return KeyCode::NumpadSubtract;
  case XK_KP_Decimal:
    return KeyCode::NumpadDecimal;
  case XK_KP_Divide:
    return KeyCode::NumpadDivide;
  case XK_KP_Separator:
    return KeyCode::NumpadSeparator;

  // OEM / punctuation
  case XK_semicolon:
    return KeyCode::Semicolon;
  case XK_equal:
    return KeyCode::Equal;
  case XK_comma:
    return KeyCode::Comma;
  case XK_minus:
    return KeyCode::Minus;
  case XK_period:
    return KeyCode::Period;
  case XK_slash:
    return KeyCode::Slash;
  case XK_grave:
    return KeyCode::GraveAccent;
  case XK_bracketleft:
    return KeyCode::LeftBracket;
  case XK_backslash:
    return KeyCode::Backslash;
  case XK_bracketright:
    return KeyCode::RightBracket;
  case XK_apostrophe:
    return KeyCode::Apostrophe;

  default:
    return KeyCode::Unknown;
  }
}

inline MouseButton X11ButtonToMouseButton(unsigned int button) noexcept
{
  switch (button)
  {
  case 1:
    return MouseButton::Left;
  case 2:
    return MouseButton::Middle;
  case 3:
    return MouseButton::Right;
  case 8:
    return MouseButton::X1;
  case 9:
    return MouseButton::X2;
  default:
    return MouseButton::Left;
  }
}

}  // namespace gecko::platform
