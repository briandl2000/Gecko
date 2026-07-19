#pragma once

#include "gecko/core/services/events.h"
#include "gecko/platform/input.h"

namespace gecko::platform {

// IInput backend that consumes window events from the platform event
// bus.  Updates a polled state cache as events arrive.  Subscribes to
// WindowKey, WindowMouseMove, WindowMouseButton, WindowMouseWheel,
// and WindowFocusChanged at construction; unsubscribes at destruction.
//
// Frame ordering expected by the app:
//   1. WindowEventInput::NewFrame()           // roll edges, clear scroll
//   2. PumpEvents()                           // OS  ->  bus
//   3. DispatchEvents()                       // bus -> state updates
//   4. App polls IsKeyDown / GetMousePosition / ...
class WindowEventInput final : public IInput
{
public:
  // Number of distinct KeyCode values we track (0x00-0xFF).
  static constexpr gecko::usize KeyCount = 256;
  // Number of distinct MouseButton values (Left/Right/Middle/X1/X2).
  static constexpr gecko::usize MouseButtonCount = 5;

  WindowEventInput() noexcept;
  ~WindowEventInput() noexcept override;

  WindowEventInput(const WindowEventInput&) = delete;
  WindowEventInput& operator=(const WindowEventInput&) = delete;

  void NewFrame() noexcept override;

  [[nodiscard]] bool IsKeyDown(KeyCode key) const noexcept override;
  [[nodiscard]] bool WasKeyPressed(KeyCode key) const noexcept override;
  [[nodiscard]] bool WasKeyReleased(KeyCode key) const noexcept override;

  [[nodiscard]] bool IsMouseButtonDown(MouseButton button) const noexcept override;
  [[nodiscard]] bool WasMouseButtonPressed(MouseButton button) const noexcept override;
  [[nodiscard]] bool WasMouseButtonReleased(MouseButton button) const noexcept override;

  [[nodiscard]] MousePosition GetMousePosition() const noexcept override;
  [[nodiscard]] MousePosition GetMousePosition(WindowHandle window) const noexcept override;
  [[nodiscard]] MousePosition GetMouseDelta() const noexcept override;
  [[nodiscard]] float GetMouseScrollX() const noexcept override;
  [[nodiscard]] float GetMouseScrollY() const noexcept override;

  [[nodiscard]] WindowHandle FocusedWindow() const noexcept override;
  [[nodiscard]] WindowHandle HoveredWindow() const noexcept override;

  [[nodiscard]] StringView GetTypedText() const noexcept override;

private:
  // Event handlers (registered as static C-style callbacks).
  static void OnKey(void* user, const gecko::EventMeta& meta, gecko::EventView view) noexcept;
  static void OnChar(void* user, const gecko::EventMeta& meta, gecko::EventView view) noexcept;
  static void OnMouseMove(void* user, const gecko::EventMeta& meta, gecko::EventView view) noexcept;
  static void OnMouseButton(void* user, const gecko::EventMeta& meta, gecko::EventView view) noexcept;
  static void OnMouseWheel(void* user, const gecko::EventMeta& meta, gecko::EventView view) noexcept;
  static void OnFocusChanged(void* user, const gecko::EventMeta& meta, gecko::EventView view) noexcept;
  static void OnMouseEntered(void* user, const gecko::EventMeta& meta, gecko::EventView view) noexcept;
  static void OnMouseExited(void* user, const gecko::EventMeta& meta, gecko::EventView view) noexcept;

  // -- State ----------------------------------------------------
  bool m_KeyDown[KeyCount] {};
  bool m_KeyDownPrev[KeyCount] {};

  bool m_MouseDown[MouseButtonCount] {};
  bool m_MouseDownPrev[MouseButtonCount] {};

  MousePosition m_MousePos {};      // current, focused-window-relative
  MousePosition m_MousePosPrev {};  // previous frame
  float m_ScrollX {0.0f};           // accumulated this frame
  float m_ScrollY {0.0f};

  WindowHandle m_FocusedWindow {};
  WindowHandle m_HoveredWindow {};
  WindowHandle m_LastMouseWindow {};
  MousePosition m_LastMouseWindowPos {};

  // Per-frame UTF-8 typed text. Cleared on NewFrame().
  StaticString<1024> m_TypedText {};

  // -- Subscriptions --------------------------------------------
  gecko::EventSubscription m_KeySub;
  gecko::EventSubscription m_CharSub;
  gecko::EventSubscription m_MouseMoveSub;
  gecko::EventSubscription m_MouseButtonSub;
  gecko::EventSubscription m_MouseWheelSub;
  gecko::EventSubscription m_FocusSub;
  gecko::EventSubscription m_MouseEnteredSub;
  gecko::EventSubscription m_MouseExitedSub;
};

}  // namespace gecko::platform
