#include "win32_monitors_backend.h"

#if defined(GECKO_PLATFORM_WINDOWS)

#include "../private/labels.h"
#include "../private/platform_utils.h"
#include "gecko/core/ptr.h"
#include "gecko/core/scope.h"
#include "gecko/core/services/events.h"
#include "gecko/core/services/log.h"
#include "gecko/platform/platform_events.h"

#include <cmath>
#include <cstring>
#include <shellscalingapi.h>

#pragma comment(lib, "Shcore.lib")

namespace gecko::platform {

Win32MonitorsBackend::Win32MonitorsBackend() noexcept
{
  GECKO_INFO(labels::General, "Win32MonitorsBackend: initialized");
}

void Win32MonitorsBackend::EnumerateMonitors() noexcept
{
  GECKO_FUNC(labels::General);
  m_Monitors.clear();
  ::EnumDisplayMonitors(nullptr, nullptr, EnumProc,
                        reinterpret_cast<LPARAM>(this));

  GECKO_INFO(labels::General, "Win32MonitorsBackend: enumerated %u monitor(s)",
             static_cast<u32>(m_Monitors.size()));
}

u32 Win32MonitorsBackend::GetMonitorCount() const noexcept
{
  return static_cast<u32>(m_Monitors.size());
}

MonitorHandle Win32MonitorsBackend::GetMonitorHandle(u32 index) const noexcept
{
  if (index >= static_cast<u32>(m_Monitors.size()))
    return {};
  return m_Monitors[index].Handle;
}

MonitorInfo Win32MonitorsBackend::GetMonitorProperties(
    MonitorHandle handle) const noexcept
{
  if (!handle.IsValid())
    return {};
  for (const auto& entry : m_Monitors)
  {
    if (entry.Handle == handle)
      return entry.Info;
  }
  return {};
}

MonitorHandle Win32MonitorsBackend::GetPrimaryMonitor() const noexcept
{
  for (const auto& entry : m_Monitors)
  {
    if (entry.Info.IsPrimary)
      return entry.Handle;
  }
  if (!m_Monitors.empty())
    return m_Monitors[0].Handle;
  return {};
}

MonitorBounds Win32MonitorsBackend::GetMonitorBounds(
    MonitorHandle handle) const noexcept
{
  MonitorInfo info = GetMonitorProperties(handle);
  return {info.Bounds, info.WorkArea};
}

void Win32MonitorsBackend::PumpEvents(
    const gecko::EventEmitter& emitter) noexcept
{
  // Win32 monitor change detection: re-enumerate and diff.
  // In a real scenario, WM_DISPLAYCHANGE is delivered to a window message
  // loop. Since the monitor backend doesn't own a window, we poll here.
  // A more advanced approach would use RegisterDeviceNotification.
  (void)emitter;
}

BOOL CALLBACK Win32MonitorsBackend::EnumProc(HMONITOR hMonitor, HDC /*hdc*/,
                                             LPRECT /*lpRect*/,
                                             LPARAM lParam) noexcept
{
  auto* self = reinterpret_cast<Win32MonitorsBackend*>(lParam);

  MONITORINFOEXA mi {};
  mi.cbSize = sizeof(mi);
  if (!::GetMonitorInfoA(hMonitor, &mi))
    return TRUE;

  MonitorEntry entry;
  entry.HMonitor = hMonitor;
  entry.Handle =
      MonitorHandle {static_cast<u64>(reinterpret_cast<uintptr_t>(hMonitor))};

  MonitorInfo& info = entry.Info;
  info.SetName(mi.szDevice);
  info.IsPrimary = (mi.dwFlags & MONITORINFOF_PRIMARY) != 0;

  info.Bounds = math::Rect2D {
      static_cast<i32>(mi.rcMonitor.left), static_cast<i32>(mi.rcMonitor.top),
      static_cast<i32>(mi.rcMonitor.right - mi.rcMonitor.left),
      static_cast<i32>(mi.rcMonitor.bottom - mi.rcMonitor.top)};

  info.WorkArea = math::Rect2D {
      static_cast<i32>(mi.rcWork.left), static_cast<i32>(mi.rcWork.top),
      static_cast<i32>(mi.rcWork.right - mi.rcWork.left),
      static_cast<i32>(mi.rcWork.bottom - mi.rcWork.top)};

  // DPI via Shcore
  UINT dpiX = 96;
  UINT dpiY = 96;
  if (SUCCEEDED(::GetDpiForMonitor(hMonitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY)))
  {
    info.Dpi = static_cast<u32>(dpiX);
    info.DpiScale = static_cast<float>(dpiX) / 96.0F;
  }

  // Refresh rate from DEVMODE
  DEVMODEA devMode {};
  devMode.dmSize = sizeof(devMode);
  if (::EnumDisplaySettingsA(mi.szDevice, ENUM_CURRENT_SETTINGS, &devMode))
  {
    if (devMode.dmDisplayFrequency > 0)
      info.RefreshRateMilliHz = devMode.dmDisplayFrequency * 1000;
  }

  self->m_Monitors.push_back(entry);
  return TRUE;
}

Unique<IMonitorsBackend> CreateWin32MonitorsBackend() noexcept
{
  return CreateUnique<Win32MonitorsBackend>();
}

}  // namespace gecko::platform

#endif  // GECKO_PLATFORM_WINDOWS
