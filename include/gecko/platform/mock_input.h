#pragma once

#include "gecko/core/api.h"
#include "gecko/platform/input.h"

#include <array>

namespace gecko::platform {

// Scriptable IInput for tests (unit tests, feature/UI tests). Expose
// public mutators so a test can drive state directly without going
// through the event bus.
//
// Frame model:
//   1. Test calls Press(...) / Release(...) / SetMousePosition(...) /
//      AddScroll(...) / SetFocusedWindow(...) / SetHoveredWindow(...).
//   2. Test calls EndFrame()  — the IInput methods now report the
//      state that was set this frame, and Was*Pressed / Was*Released
//      report edges relative to the previous EndFrame.
//
// Equivalent to NewFrame() being called *before* the next test step:
// EndFrame() advances the frame and rolls current → previous so the
// next round of mutations becomes the new "current" frame.
class GECKO_API MockInput final : public IInput
{
public:
  static constexpr ::gecko::usize KeyCount = 256;
  static constexpr ::gecko::usize MouseButtonCount = 5;

  MockInput() noexcept = default;

  // ── IInput ─────────────────────────────────────────────────
  void NewFrame() noexcept override
  {
    EndFrame();
  }

  [[nodiscard]] bool IsKeyDown(KeyCode key) const noexcept override
  {
    return m_KeyDown[KeyIdx(key)];
  }
  [[nodiscard]] bool WasKeyPressed(KeyCode key) const noexcept override
  {
    const auto i = KeyIdx(key);
    return m_KeyDown[i] && !m_KeyDownPrev[i];
  }
  [[nodiscard]] bool WasKeyReleased(KeyCode key) const noexcept override
  {
    const auto i = KeyIdx(key);
    return !m_KeyDown[i] && m_KeyDownPrev[i];
  }

  [[nodiscard]] bool IsMouseButtonDown(
      MouseButton button) const noexcept override
  {
    return m_MouseDown[ButtonIdx(button)];
  }
  [[nodiscard]] bool WasMouseButtonPressed(
      MouseButton button) const noexcept override
  {
    const auto i = ButtonIdx(button);
    return m_MouseDown[i] && !m_MouseDownPrev[i];
  }
  [[nodiscard]] bool WasMouseButtonReleased(
      MouseButton button) const noexcept override
  {
    const auto i = ButtonIdx(button);
    return !m_MouseDown[i] && m_MouseDownPrev[i];
  }

  [[nodiscard]] MousePosition GetMousePosition() const noexcept override
  {
    return m_MousePos;
  }
  [[nodiscard]] MousePosition GetMousePosition(
      WindowHandle window) const noexcept override
  {
    if (window.IsValid() && window == m_MouseWindow)
      return m_MousePos;
    return {};
  }
  [[nodiscard]] MousePosition GetMouseDelta() const noexcept override
  {
    return MousePosition {m_MousePos.X - m_MousePosPrev.X,
                          m_MousePos.Y - m_MousePosPrev.Y};
  }
  [[nodiscard]] float GetMouseScrollX() const noexcept override
  {
    return m_ScrollX;
  }
  [[nodiscard]] float GetMouseScrollY() const noexcept override
  {
    return m_ScrollY;
  }

  [[nodiscard]] WindowHandle FocusedWindow() const noexcept override
  {
    return m_FocusedWindow;
  }
  [[nodiscard]] WindowHandle HoveredWindow() const noexcept override
  {
    return m_HoveredWindow;
  }

  // ── Test mutators ──────────────────────────────────────────
  void PressKey(KeyCode key) noexcept
  {
    m_KeyDown[KeyIdx(key)] = true;
  }
  void ReleaseKey(KeyCode key) noexcept
  {
    m_KeyDown[KeyIdx(key)] = false;
  }
  void PressMouseButton(MouseButton b) noexcept
  {
    m_MouseDown[ButtonIdx(b)] = true;
  }
  void ReleaseMouseButton(MouseButton b) noexcept
  {
    m_MouseDown[ButtonIdx(b)] = false;
  }
  void SetMousePosition(MousePosition pos, WindowHandle window = {}) noexcept
  {
    m_MousePos = pos;
    m_MouseWindow = window;
  }
  void AddScroll(float dx, float dy) noexcept
  {
    m_ScrollX += dx;
    m_ScrollY += dy;
  }
  void SetFocusedWindow(WindowHandle w) noexcept
  {
    m_FocusedWindow = w;
  }
  void SetHoveredWindow(WindowHandle w) noexcept
  {
    m_HoveredWindow = w;
  }

  // Roll current → previous, clear scroll. Equivalent to NewFrame().
  void EndFrame() noexcept
  {
    m_KeyDownPrev = m_KeyDown;
    m_MouseDownPrev = m_MouseDown;
    m_MousePosPrev = m_MousePos;
    m_ScrollX = 0.0f;
    m_ScrollY = 0.0f;
  }

  // Reset everything to zero.
  void Reset() noexcept
  {
    m_KeyDown = {};
    m_KeyDownPrev = {};
    m_MouseDown = {};
    m_MouseDownPrev = {};
    m_MousePos = {};
    m_MousePosPrev = {};
    m_ScrollX = 0.0f;
    m_ScrollY = 0.0f;
    m_MouseWindow = {};
    m_FocusedWindow = {};
    m_HoveredWindow = {};
  }

private:
  [[nodiscard]] static ::gecko::usize KeyIdx(KeyCode k) noexcept
  {
    const auto v = static_cast<::gecko::usize>(k);
    return v < KeyCount ? v : 0;
  }
  [[nodiscard]] static ::gecko::usize ButtonIdx(MouseButton b) noexcept
  {
    const auto v = static_cast<::gecko::usize>(b);
    return v < MouseButtonCount ? v : 0;
  }

  ::std::array<bool, KeyCount> m_KeyDown {};
  ::std::array<bool, KeyCount> m_KeyDownPrev {};
  ::std::array<bool, MouseButtonCount> m_MouseDown {};
  ::std::array<bool, MouseButtonCount> m_MouseDownPrev {};
  MousePosition m_MousePos {};
  MousePosition m_MousePosPrev {};
  float m_ScrollX {0.0f};
  float m_ScrollY {0.0f};
  WindowHandle m_MouseWindow {};
  WindowHandle m_FocusedWindow {};
  WindowHandle m_HoveredWindow {};
};

}  // namespace gecko::platform
