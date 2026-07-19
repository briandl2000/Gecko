#include "null_windows_interface.h"

#include "gecko/core/ptr.h"
#include "gecko/core/scope.h"
#include "gecko/core/services/events.h"
#include "gecko/core/services/log.h"
#include "labels.h"
#include "platform_utils.h"

namespace gecko::platform {

WindowHandle NullWindowsBackend::CreateWindow(const WindowDesc& desc) noexcept
{
  GECKO_SCOPE(labels::General);

  const u64 id = ++m_NextId;

  NullWindowEntry entry;
  entry.Desc = desc;
  entry.TitleStorage = desc.Title ? desc.Title : "";
  entry.Desc.Title = entry.TitleStorage.c_str();
  entry.ClientSize = {static_cast<u32>(desc.Size.X), static_cast<u32>(desc.Size.Y)};
  entry.Decorated = desc.Decorated;
  entry.Resizable = desc.Resizable;
  entry.Mode = desc.Mode;
  entry.Buttons = desc.Buttons;
  entry.State = desc.Visible ? platform::WindowState::Normal : platform::WindowState::Hidden;
  entry.Alive = true;

  auto [it, ok] = m_Windows.emplace(id, Move(entry));
  it->second.Desc.Title = it->second.TitleStorage.c_str();

  GECKO_INFO(labels::General, "Created null window id={}", static_cast<unsigned long long>(id));
  return WindowHandle {id};
}

void NullWindowsBackend::DestroyWindow(WindowHandle window) noexcept
{
  GECKO_SCOPE(labels::General);
  if (!window.IsValid())
    return;

  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return;

  it->second.Alive = false;

  StagedEvent ev;
  ev.Code = events::WindowClosed;
  ev.Data.Closed = {window, NowNsSafe()};
  ev.PayloadSize = static_cast<u32>(sizeof(events::WindowClosedPayload));
  m_Staged.push_back(ev);

  m_Windows.erase(it);
}

bool NullWindowsBackend::IsWindowAlive(WindowHandle window) const noexcept
{
  if (!window.IsValid())
    return false;
  return m_Windows.find(window.Id) != m_Windows.end();
}

bool NullWindowsBackend::RequestClose(WindowHandle window) noexcept
{
  GECKO_SCOPE(labels::General);
  if (!window.IsValid() || !IsWindowAlive(window))
    return false;

  StagedEvent ev;
  ev.Code = events::WindowCloseRequested;
  ev.Data.CloseRequested = {window, NowNsSafe()};
  ev.PayloadSize = static_cast<u32>(sizeof(events::WindowCloseRequestedPayload));
  m_Staged.push_back(ev);
  return true;
}

void NullWindowsBackend::PumpEvents(const gecko::EventEmitter& emitter) noexcept
{
  for (const auto& ev : m_Staged)
    gecko::SendEvent(emitter, ev.Code, gecko::EventView {&ev.Data, ev.PayloadSize});
  m_Staged.clear();
}

Extent2D NullWindowsBackend::GetClientSize(WindowHandle window) const noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return Extent2D {};
  return it->second.ClientSize;
}

void NullWindowsBackend::SetTitle(WindowHandle window, const char* title) noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return;
  it->second.TitleStorage = title ? title : "";
  it->second.Desc.Title = it->second.TitleStorage.c_str();
}

DpiInfo NullWindowsBackend::GetDpi(WindowHandle window) const noexcept
{
  (void)window;
  return DpiInfo {};
}

NativeWindowHandle NullWindowsBackend::GetNativeWindowHandle(WindowHandle window) const noexcept
{
  (void)window;
  return NativeWindowHandle {};
}

void NullWindowsBackend::SetClientSize(WindowHandle window, Extent2D size) noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return;
  it->second.ClientSize = size;
}

const char* NullWindowsBackend::GetTitle(WindowHandle window) const noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return "";
  return it->second.TitleStorage.c_str();
}

void NullWindowsBackend::SetPosition(WindowHandle window, math::Int2 pos) noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return;

  const auto oldPos = it->second.Position;
  it->second.Position = pos;

  if (oldPos.X != pos.X || oldPos.Y != pos.Y)
  {
    StagedEvent ev;
    ev.Code = events::WindowMoved;
    ev.Data.Moved = {window, NowNsSafe(), pos.X, pos.Y};
    ev.PayloadSize = static_cast<u32>(sizeof(events::WindowMovedPayload));
    m_Staged.push_back(ev);
  }
}

math::Int2 NullWindowsBackend::GetPosition(WindowHandle window) const noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return math::Int2 {0, 0};
  return it->second.Position;
}

void NullWindowsBackend::SetWindowState(WindowHandle window, platform::WindowState state) noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return;

  const auto oldState = it->second.State;
  it->second.State = state;

  if (oldState != state)
  {
    StagedEvent ev;
    ev.Code = events::WindowStateChanged;
    ev.Data.StateChanged = {window, NowNsSafe(), oldState, state};
    ev.PayloadSize = static_cast<u32>(sizeof(events::WindowStateChangedPayload));
    m_Staged.push_back(ev);
  }
}

platform::WindowState NullWindowsBackend::GetWindowState(WindowHandle window) const noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return platform::WindowState::Normal;
  return it->second.State;
}

void NullWindowsBackend::SetDecorated(WindowHandle window, bool decorated) noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return;
  it->second.Decorated = decorated;
}

bool NullWindowsBackend::IsDecorated(WindowHandle window) const noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return true;
  return it->second.Decorated;
}

void NullWindowsBackend::RequestFocus(WindowHandle window) noexcept
{
  (void)window;
  // Null backend: no-op, focus is not meaningful without a display.
}

void NullWindowsBackend::SetResizable(WindowHandle window, bool resizable) noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return;
  it->second.Resizable = resizable;
}

bool NullWindowsBackend::IsResizable(WindowHandle window) const noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return true;
  return it->second.Resizable;
}

void NullWindowsBackend::SetWindowMode(WindowHandle window, WindowMode mode) noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return;
  it->second.Mode = mode;
}

WindowMode NullWindowsBackend::GetWindowMode(WindowHandle window) const noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return WindowMode::Windowed;
  return it->second.Mode;
}

void NullWindowsBackend::SetWindowButtons(WindowHandle window, WindowButtons buttons) noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return;
  it->second.Buttons = buttons;
}

WindowButtons NullWindowsBackend::GetWindowButtons(WindowHandle window) const noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return WindowButtons::All;
  return it->second.Buttons;
}

void NullWindowsBackend::SetMinSize(WindowHandle window, Extent2D size) noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return;
  it->second.MinSize = size;
}

void NullWindowsBackend::SetMaxSize(WindowHandle window, Extent2D size) noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return;
  it->second.MaxSize = size;
}

void NullWindowsBackend::SetAlwaysOnTop(WindowHandle window, bool topmost) noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return;
  it->second.AlwaysOnTop = topmost;
}

bool NullWindowsBackend::IsAlwaysOnTop(WindowHandle window) const noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return false;
  return it->second.AlwaysOnTop;
}

void NullWindowsBackend::SetCursorMode(WindowHandle window, CursorMode mode) noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return;
  it->second.Cursor = mode;
}

CursorMode NullWindowsBackend::GetCursorMode(WindowHandle window) const noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return CursorMode::Normal;
  return it->second.Cursor;
}

u64 NullWindowsBackend::NowNsSafe() noexcept
{
  return gecko::platform::NowNsSafe();
}

Unique<IWindowsBackend> CreateNullWindowsBackend() noexcept
{
  return CreateUnique<NullWindowsBackend>();
}

}  // namespace gecko::platform
