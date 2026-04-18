#pragma once

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "gecko/platform/monitors_interface.h"

#include <vector>
#include <Windows.h>

namespace gecko::platform {

struct MonitorEntry
{
  MonitorHandle Handle {};
  MonitorInfo Info {};
  HMONITOR HMonitor {nullptr};
};

class Win32MonitorsBackend final : public IMonitorsBackend
{
public:
  Win32MonitorsBackend() noexcept;
  ~Win32MonitorsBackend() noexcept override = default;

  void EnumerateMonitors() noexcept override;
  u32 GetMonitorCount() const noexcept override;
  MonitorHandle GetMonitorHandle(u32 index) const noexcept override;
  MonitorInfo GetMonitorProperties(
      MonitorHandle handle) const noexcept override;
  MonitorHandle GetPrimaryMonitor() const noexcept override;
  MonitorBounds GetMonitorBounds(MonitorHandle handle) const noexcept override;
  void PumpEvents(const gecko::EventEmitter& emitter) noexcept override;

private:
  static BOOL CALLBACK EnumProc(HMONITOR hMonitor, HDC hdc, LPRECT lpRect,
                                LPARAM lParam) noexcept;

  ::std::vector<MonitorEntry> m_Monitors;
};

Unique<IMonitorsBackend> CreateWin32MonitorsBackend() noexcept;

}  // namespace gecko::platform

#endif  // _WIN32
