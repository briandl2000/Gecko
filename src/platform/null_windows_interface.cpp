#include "null_windows_interface.h"

#include "gecko/core/ptr.h"
#include "gecko/core/scope.h"
#include "gecko/core/services/log.h"
#include "gecko/platform/window.h"
#include "private/labels.h"

namespace gecko::platform {

bool NullWindowsBackend::CreateWindow(const WindowDesc& desc,
                                      WindowHandle& outWindow) noexcept
{
  GECKO_FUNC(labels::General);

  const u64 id = ++m_NextId;
  outWindow = WindowHandle {id};

  WindowState st;
  st.Desc = desc;
  st.ClientSize = desc.Size;
  st.Alive = true;

  m_Windows.emplace(id, st);

  GECKO_INFO(labels::General, "Created null window id=%llu\n",
             static_cast<unsigned long long>(id));
  return true;
}

void NullWindowsBackend::DestroyWindow(WindowHandle window) noexcept
{
  GECKO_FUNC(labels::General);
  if (!window.IsValid())
    return;

  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return;

  it->second.Alive = false;

  WindowEvent ev {};
  ev.Kind = WindowEventKind::Closed;
  ev.Window = window;
  ev.TimeNs = NowNsSafe();
  m_Events.push_back(ev);

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
  GECKO_FUNC(labels::General);
  if (!window.IsValid())
    return false;
  if (!IsWindowAlive(window))
    return false;

  WindowEvent ev {};
  ev.Kind = WindowEventKind::CloseRequested;
  ev.Window = window;
  ev.TimeNs = NowNsSafe();
  m_Events.push_back(ev);
  return true;
}

void NullWindowsBackend::PumpEvents() noexcept
{
  GECKO_FUNC(labels::General);
  // Null backend: no OS events.
}

bool NullWindowsBackend::PollEvent(WindowEvent& outEvent) noexcept
{
  if (m_Events.empty())
    return false;
  outEvent = m_Events.front();
  m_Events.pop_front();
  return true;
}

Extent2D NullWindowsBackend::GetClientSize(WindowHandle window) const noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return Extent2D {};
  return it->second.ClientSize;
}

void NullWindowsBackend::SetTitle(WindowHandle window,
                                  const char* title) noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return;
  it->second.Desc.Title = title;
}

DpiInfo NullWindowsBackend::GetDpi(WindowHandle window) const noexcept
{
  (void)window;
  return DpiInfo {};
}

NativeWindowHandle NullWindowsBackend::GetNativeWindowHandle(
    WindowHandle window) const noexcept
{
  (void)window;
  return NativeWindowHandle {};
}

u64 NullWindowsBackend::NowNsSafe() noexcept
{
  if (auto* profiler = GetProfiler())
    return profiler->NowNs();
  return 0;
}

Unique<IWindowsBackend> CreateNullWindowsBackend() noexcept
{
  return CreateUnique<NullWindowsBackend>();
}

}  // namespace gecko::platform
