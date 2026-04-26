#include "private/window_event_input.h"

#include "gecko/platform/platform_events.h"

namespace gecko::platform {

namespace {

// Maps MouseButton enum value (u8) to slot index. The enum values
// happen to match {0..4} already.
[[nodiscard]] inline ::gecko::usize ButtonIndex(MouseButton b) noexcept
{
  const auto v = static_cast<::gecko::usize>(b);
  return v < WindowEventInput::MouseButtonCount ? v : 0;
}

[[nodiscard]] inline ::gecko::usize KeyIndex(KeyCode k) noexcept
{
  const auto v = static_cast<::gecko::usize>(k);
  return v < WindowEventInput::KeyCount ? v : 0;
}

}  // namespace

WindowEventInput::WindowEventInput() noexcept
{
  m_KeySub = ::gecko::SubscribeEvent(events::WindowKey, &OnKey, this);
  m_CharSub = ::gecko::SubscribeEvent(events::WindowChar, &OnChar, this);
  m_MouseMoveSub =
      ::gecko::SubscribeEvent(events::WindowMouseMove, &OnMouseMove, this);
  m_MouseButtonSub =
      ::gecko::SubscribeEvent(events::WindowMouseButton, &OnMouseButton, this);
  m_MouseWheelSub =
      ::gecko::SubscribeEvent(events::WindowMouseWheel, &OnMouseWheel, this);
  m_FocusSub = ::gecko::SubscribeEvent(events::WindowFocusChanged,
                                       &OnFocusChanged, this);
  m_MouseEnteredSub = ::gecko::SubscribeEvent(events::WindowMouseEntered,
                                              &OnMouseEntered, this);
  m_MouseExitedSub =
      ::gecko::SubscribeEvent(events::WindowMouseExited, &OnMouseExited, this);
}

WindowEventInput::~WindowEventInput() noexcept = default;

void WindowEventInput::NewFrame() noexcept
{
  m_KeyDownPrev = m_KeyDown;
  m_MouseDownPrev = m_MouseDown;
  m_MousePosPrev = m_MousePos;
  m_ScrollX = 0.0f;
  m_ScrollY = 0.0f;
  m_TypedText.clear();
}

bool WindowEventInput::IsKeyDown(KeyCode key) const noexcept
{
  return m_KeyDown[KeyIndex(key)];
}

bool WindowEventInput::WasKeyPressed(KeyCode key) const noexcept
{
  const auto i = KeyIndex(key);
  return m_KeyDown[i] && !m_KeyDownPrev[i];
}

bool WindowEventInput::WasKeyReleased(KeyCode key) const noexcept
{
  const auto i = KeyIndex(key);
  return !m_KeyDown[i] && m_KeyDownPrev[i];
}

bool WindowEventInput::IsMouseButtonDown(MouseButton b) const noexcept
{
  return m_MouseDown[ButtonIndex(b)];
}

bool WindowEventInput::WasMouseButtonPressed(MouseButton b) const noexcept
{
  const auto i = ButtonIndex(b);
  return m_MouseDown[i] && !m_MouseDownPrev[i];
}

bool WindowEventInput::WasMouseButtonReleased(MouseButton b) const noexcept
{
  const auto i = ButtonIndex(b);
  return !m_MouseDown[i] && m_MouseDownPrev[i];
}

MousePosition WindowEventInput::GetMousePosition() const noexcept
{
  return m_MousePos;
}

MousePosition WindowEventInput::GetMousePosition(
    WindowHandle window) const noexcept
{
  if (window.IsValid() && window == m_LastMouseWindow)
    return m_LastMouseWindowPos;
  return {};
}

MousePosition WindowEventInput::GetMouseDelta() const noexcept
{
  return MousePosition {m_MousePos.X - m_MousePosPrev.X,
                        m_MousePos.Y - m_MousePosPrev.Y};
}

float WindowEventInput::GetMouseScrollX() const noexcept
{
  return m_ScrollX;
}

float WindowEventInput::GetMouseScrollY() const noexcept
{
  return m_ScrollY;
}

WindowHandle WindowEventInput::FocusedWindow() const noexcept
{
  return m_FocusedWindow;
}

WindowHandle WindowEventInput::HoveredWindow() const noexcept
{
  return m_HoveredWindow;
}

::std::string_view WindowEventInput::GetTypedText() const noexcept
{
  return m_TypedText;
}

// ── Event handlers ──────────────────────────────────────────────

void WindowEventInput::OnKey(void* user, const ::gecko::EventMeta&,
                             ::gecko::EventView view) noexcept
{
  auto* self = static_cast<WindowEventInput*>(user);
  const auto* p =
      reinterpret_cast<const events::WindowKeyPayload*>(view.Data());
  self->m_KeyDown[KeyIndex(p->Key)] = (p->Down != 0);
}

void WindowEventInput::OnChar(void* user, const ::gecko::EventMeta&,
                              ::gecko::EventView view) noexcept
{
  auto* self = static_cast<WindowEventInput*>(user);
  const auto* p =
      reinterpret_cast<const events::WindowCharPayload*>(view.Data());
  const ::gecko::u32 cp = p->Codepoint;

  // Encode the codepoint as UTF-8 and append to the per-frame buffer.
  char buf[4];
  if (cp < 0x80)
  {
    self->m_TypedText.push_back(static_cast<char>(cp));
  }
  else if (cp < 0x800)
  {
    buf[0] = static_cast<char>(0xC0 | (cp >> 6));
    buf[1] = static_cast<char>(0x80 | (cp & 0x3F));
    self->m_TypedText.append(buf, 2);
  }
  else if (cp < 0x10000)
  {
    buf[0] = static_cast<char>(0xE0 | (cp >> 12));
    buf[1] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    buf[2] = static_cast<char>(0x80 | (cp & 0x3F));
    self->m_TypedText.append(buf, 3);
  }
  else if (cp < 0x110000)
  {
    buf[0] = static_cast<char>(0xF0 | (cp >> 18));
    buf[1] = static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
    buf[2] = static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
    buf[3] = static_cast<char>(0x80 | (cp & 0x3F));
    self->m_TypedText.append(buf, 4);
  }
}

void WindowEventInput::OnMouseMove(void* user, const ::gecko::EventMeta&,
                                   ::gecko::EventView view) noexcept
{
  auto* self = static_cast<WindowEventInput*>(user);
  const auto* p =
      reinterpret_cast<const events::WindowMouseMovePayload*>(view.Data());
  self->m_LastMouseWindow = p->Window;
  self->m_LastMouseWindowPos = MousePosition {p->X, p->Y};
  if (p->Window == self->m_FocusedWindow || !self->m_FocusedWindow.IsValid())
    self->m_MousePos = MousePosition {p->X, p->Y};
}

void WindowEventInput::OnMouseButton(void* user, const ::gecko::EventMeta&,
                                     ::gecko::EventView view) noexcept
{
  auto* self = static_cast<WindowEventInput*>(user);
  const auto* p =
      reinterpret_cast<const events::WindowMouseButtonPayload*>(view.Data());
  self->m_MouseDown[ButtonIndex(p->Button)] = (p->Down != 0);
}

void WindowEventInput::OnMouseWheel(void* user, const ::gecko::EventMeta&,
                                    ::gecko::EventView view) noexcept
{
  auto* self = static_cast<WindowEventInput*>(user);
  const auto* p =
      reinterpret_cast<const events::WindowMouseWheelPayload*>(view.Data());
  self->m_ScrollX += p->DeltaX;
  self->m_ScrollY += p->DeltaY;
}

void WindowEventInput::OnFocusChanged(void* user, const ::gecko::EventMeta&,
                                      ::gecko::EventView view) noexcept
{
  auto* self = static_cast<WindowEventInput*>(user);
  const auto* p =
      reinterpret_cast<const events::WindowFocusChangedPayload*>(view.Data());
  if (p->Focused)
    self->m_FocusedWindow = p->Window;
  else if (self->m_FocusedWindow == p->Window)
    self->m_FocusedWindow = {};
}

void WindowEventInput::OnMouseEntered(void* user, const ::gecko::EventMeta&,
                                      ::gecko::EventView view) noexcept
{
  auto* self = static_cast<WindowEventInput*>(user);
  const auto* p =
      reinterpret_cast<const events::WindowMouseEnteredPayload*>(view.Data());
  self->m_HoveredWindow = p->Window;
}

void WindowEventInput::OnMouseExited(void* user, const ::gecko::EventMeta&,
                                     ::gecko::EventView view) noexcept
{
  auto* self = static_cast<WindowEventInput*>(user);
  const auto* p =
      reinterpret_cast<const events::WindowMouseExitedPayload*>(view.Data());
  if (self->m_HoveredWindow == p->Window)
    self->m_HoveredWindow = {};
}

}  // namespace gecko::platform
