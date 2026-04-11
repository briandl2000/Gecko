#include "null_windows_interface.h"

#include "gecko/core/ptr.h"
#include "gecko/core/scope.h"
#include "gecko/core/services/events.h"
#include "gecko/core/services/log.h"
#include "labels.h"
#include "platform_utils.h"

namespace gecko::platform {

bool NullWindowsBackend::CreateWindow(const WindowDesc& desc,
                                      WindowHandle& outWindow) noexcept
{
  GECKO_FUNC(labels::General);

  const u64 id = ++m_NextId;
  outWindow = WindowHandle {id};

  WindowState st;
  st.Desc = desc;
  st.ClientSize = {static_cast<u32>(desc.Size.X),
                   static_cast<u32>(desc.Size.Y)};
  st.Alive = true;

  m_Windows.emplace(id, st);

  GECKO_INFO(labels::General, "Created null window id=%llu",
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
  GECKO_FUNC(labels::General);
  if (!window.IsValid() || !IsWindowAlive(window))
    return false;

  StagedEvent ev;
  ev.Code = events::WindowCloseRequested;
  ev.Data.CloseRequested = {window, NowNsSafe()};
  ev.PayloadSize =
      static_cast<u32>(sizeof(events::WindowCloseRequestedPayload));
  m_Staged.push_back(ev);
  return true;
}

void NullWindowsBackend::PumpEvents(const gecko::EventEmitter& emitter) noexcept
{
  for (const auto& ev : m_Staged)
    gecko::SendEvent(emitter, ev.Code,
                     gecko::EventView {&ev.Data, ev.PayloadSize});
  m_Staged.clear();
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
  return gecko::platform::NowNsSafe();
}

Unique<IWindowsBackend> CreateNullWindowsBackend() noexcept
{
  return CreateUnique<NullWindowsBackend>();
}

}  // namespace gecko::platform
