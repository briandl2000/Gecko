#include "gecko/platform/input.h"

namespace gecko::platform {

bool IsKeyDown(KeyCode key) noexcept
{
  if (auto* in = GetInput())
    return in->IsKeyDown(key);
  return false;
}

bool WasKeyPressed(KeyCode key) noexcept
{
  if (auto* in = GetInput())
    return in->WasKeyPressed(key);
  return false;
}

bool WasKeyReleased(KeyCode key) noexcept
{
  if (auto* in = GetInput())
    return in->WasKeyReleased(key);
  return false;
}

bool IsMouseButtonDown(MouseButton button) noexcept
{
  if (auto* in = GetInput())
    return in->IsMouseButtonDown(button);
  return false;
}

bool WasMouseButtonPressed(MouseButton button) noexcept
{
  if (auto* in = GetInput())
    return in->WasMouseButtonPressed(button);
  return false;
}

bool WasMouseButtonReleased(MouseButton button) noexcept
{
  if (auto* in = GetInput())
    return in->WasMouseButtonReleased(button);
  return false;
}

MousePosition GetMousePosition() noexcept
{
  if (auto* in = GetInput())
    return in->GetMousePosition();
  return {};
}

MousePosition GetMousePosition(WindowHandle window) noexcept
{
  if (auto* in = GetInput())
    return in->GetMousePosition(window);
  return {};
}

MousePosition GetMouseDelta() noexcept
{
  if (auto* in = GetInput())
    return in->GetMouseDelta();
  return {};
}

float GetMouseScrollX() noexcept
{
  if (auto* in = GetInput())
    return in->GetMouseScrollX();
  return 0.0f;
}

float GetMouseScrollY() noexcept
{
  if (auto* in = GetInput())
    return in->GetMouseScrollY();
  return 0.0f;
}

WindowHandle FocusedWindow() noexcept
{
  if (auto* in = GetInput())
    return in->FocusedWindow();
  return {};
}

WindowHandle HoveredWindow() noexcept
{
  if (auto* in = GetInput())
    return in->HoveredWindow();
  return {};
}

::gecko::StringView GetTypedText() noexcept
{
  if (auto* in = GetInput())
    return in->GetTypedText();
  return {};
}

}  // namespace gecko::platform
