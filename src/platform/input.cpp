#include "gecko/platform/input.h"

namespace gecko::platform {

namespace {

inline IInput* SafeInput() noexcept
{
  return GetInput();
}

}  // namespace

bool IsKeyDown(KeyCode key) noexcept
{
  if (auto* in = SafeInput())
    return in->IsKeyDown(key);
  return false;
}

bool WasKeyPressed(KeyCode key) noexcept
{
  if (auto* in = SafeInput())
    return in->WasKeyPressed(key);
  return false;
}

bool WasKeyReleased(KeyCode key) noexcept
{
  if (auto* in = SafeInput())
    return in->WasKeyReleased(key);
  return false;
}

bool IsMouseButtonDown(MouseButton button) noexcept
{
  if (auto* in = SafeInput())
    return in->IsMouseButtonDown(button);
  return false;
}

bool WasMouseButtonPressed(MouseButton button) noexcept
{
  if (auto* in = SafeInput())
    return in->WasMouseButtonPressed(button);
  return false;
}

bool WasMouseButtonReleased(MouseButton button) noexcept
{
  if (auto* in = SafeInput())
    return in->WasMouseButtonReleased(button);
  return false;
}

MousePosition GetMousePosition() noexcept
{
  if (auto* in = SafeInput())
    return in->GetMousePosition();
  return {};
}

MousePosition GetMouseDelta() noexcept
{
  if (auto* in = SafeInput())
    return in->GetMouseDelta();
  return {};
}

float GetMouseScrollY() noexcept
{
  if (auto* in = SafeInput())
    return in->GetMouseScrollY();
  return 0.0f;
}

WindowHandle FocusedWindow() noexcept
{
  if (auto* in = SafeInput())
    return in->FocusedWindow();
  return {};
}

WindowHandle HoveredWindow() noexcept
{
  if (auto* in = SafeInput())
    return in->HoveredWindow();
  return {};
}

::std::string_view GetTypedText() noexcept
{
  if (auto* in = SafeInput())
    return in->GetTypedText();
  return {};
}

}  // namespace gecko::platform
