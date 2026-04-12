#include "gecko/platform/platform_config.h"

#if defined(_WIN32)

#include "../private/labels.h"
#include "../private/platform_utils.h"
#include "gecko/core/ptr.h"
#include "gecko/core/scope.h"
#include "gecko/core/services/events.h"
#include "gecko/core/services/log.h"
#include "gecko/platform/monitors_interface.h"
#include "gecko/platform/platform_events.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <cmath>
#include <cstring>
#include <shellscalingapi.h>
#include <vector>
#include <Windows.h>

#pragma comment(lib, "Shcore.lib")

namespace gecko::platform {

namespace {

struct MonitorEntry
{
  MonitorHandle Handle {};
  MonitorInfo Info {};
  HMONITOR HMonitor {nullptr};
};

}  // namespace

class Win32MonitorsBackend final : public IMonitorsBackend
{
public:
  Win32MonitorsBackend() noexcept
  {
    GECKO_INFO(labels::General, "Win32MonitorsBackend: initialized");
  }

  ~Win32MonitorsBackend() noexcept override = default;

  void EnumerateMonitors() noexcept override
  {
    GECKO_FUNC(labels::General);
    m_Monitors.clear();
    ::EnumDisplayMonitors(nullptr, nullptr, EnumProc,
                          reinterpret_cast<LPARAM>(this));

    GECKO_INFO(labels::General,
               "Win32MonitorsBackend: enumerated %u monitor(s)",
               static_cast<u32>(m_Monitors.size()));
  }

  u32 GetMonitorCount() const noexcept override
  {
    return static_cast<u32>(m_Monitors.size());
  }

  MonitorHandle GetMonitorHandle(u32 index) const noexcept override
  {
    if (index >= static_cast<u32>(m_Monitors.size()))
      return {};
    return m_Monitors[index].Handle;
  }

  MonitorInfo GetMonitorProperties(MonitorHandle handle) const noexcept override
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

  MonitorHandle GetPrimaryMonitor() const noexcept override
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

  MonitorBounds GetMonitorBounds(MonitorHandle handle) const noexcept override
  {
    MonitorInfo info = GetMonitorProperties(handle);
    return {info.Bounds, info.WorkArea};
  }

  void PumpEvents(const gecko::EventEmitter& emitter) noexcept override
  {
    // Win32 monitor change detection: re-enumerate and diff.
    // In a real scenario, WM_DISPLAYCHANGE is delivered to a window message
    // loop. Since the monitor backend doesn't own a window, we poll here.
    // A more advanced approach would use RegisterDeviceNotification.
    (void)emitter;
  }

private:
  static BOOL CALLBACK EnumProc(HMONITOR hMonitor, HDC /*hdc*/,
                                LPRECT /*lpRect*/, LPARAM lParam) noexcept
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
    if (SUCCEEDED(
            ::GetDpiForMonitor(hMonitor, MDT_EFFECTIVE_DPI, &dpiX, &dpiY)))
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

  std::vector<MonitorEntry> m_Monitors;
};

Unique<IMonitorsBackend> CreateWin32MonitorsBackend() noexcept
{
  return CreateUnique<Win32MonitorsBackend>();
}

}  // namespace gecko::platform

#endif  // _WIN32
