#pragma once

#include "gecko/platform/input.h"

#include <string_view>

namespace gecko::platform {

// No-op input service used when nothing else is published. Returns
// false/zero/invalid for every query.
class NullInput final : public IInput
{
public:
  void NewFrame() noexcept override
  {}

  [[nodiscard]] bool IsKeyDown(KeyCode) const noexcept override
  {
    return false;
  }
  [[nodiscard]] bool WasKeyPressed(KeyCode) const noexcept override
  {
    return false;
  }
  [[nodiscard]] bool WasKeyReleased(KeyCode) const noexcept override
  {
    return false;
  }

  [[nodiscard]] bool IsMouseButtonDown(MouseButton) const noexcept override
  {
    return false;
  }
  [[nodiscard]] bool WasMouseButtonPressed(MouseButton) const noexcept override
  {
    return false;
  }
  [[nodiscard]] bool WasMouseButtonReleased(MouseButton) const noexcept override
  {
    return false;
  }

  [[nodiscard]] MousePosition GetMousePosition() const noexcept override
  {
    return {};
  }
  [[nodiscard]] MousePosition GetMousePosition(
      WindowHandle) const noexcept override
  {
    return {};
  }
  [[nodiscard]] MousePosition GetMouseDelta() const noexcept override
  {
    return {};
  }
  [[nodiscard]] float GetMouseScrollX() const noexcept override
  {
    return 0.0f;
  }
  [[nodiscard]] float GetMouseScrollY() const noexcept override
  {
    return 0.0f;
  }

  [[nodiscard]] WindowHandle FocusedWindow() const noexcept override
  {
    return {};
  }
  [[nodiscard]] WindowHandle HoveredWindow() const noexcept override
  {
    return {};
  }
  [[nodiscard]] ::std::string_view GetTypedText() const noexcept override
  {
    return {};
  }
};

}  // namespace gecko::platform
