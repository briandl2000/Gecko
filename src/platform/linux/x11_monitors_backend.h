#pragma once

#if defined(GECKO_PLATFORM_LINUX) && defined(GECKO_PLATFORM_LINUX_X11)

#include "gecko/platform/monitors_interface.h"

#include <vector>

// Forward-declare X11/XRandR types to avoid macro conflicts (X11 defines
// "Always" which clashes with gecko enums).
using RROutput = unsigned long;
struct _XDisplay;
using Display = _XDisplay;

namespace gecko::platform {

struct MonitorEntry
{
  MonitorHandle Handle {};
  MonitorInfo Info {};
  RROutput OutputId {0};
};

class X11MonitorsBackend final : public IMonitorsBackend
{
public:
  X11MonitorsBackend() noexcept;
  ~X11MonitorsBackend() noexcept override;

  void EnumerateMonitors() noexcept override;
  u32 GetMonitorCount() const noexcept override;
  MonitorHandle GetMonitorHandle(u32 index) const noexcept override;
  MonitorInfo GetMonitorProperties(MonitorHandle handle) const noexcept override;
  MonitorHandle GetPrimaryMonitor() const noexcept override;
  MonitorBounds GetMonitorBounds(MonitorHandle handle) const noexcept override;
  void PumpEvents(const gecko::EventEmitter& emitter) noexcept override;

private:
  void HandleScreenChange(const gecko::EventEmitter& emitter) noexcept;

  Display* m_Display {nullptr};
  int m_RREventBase {-1};
  std::vector<MonitorEntry> m_Monitors;
};

Unique<IMonitorsBackend> CreateXlibMonitorsBackend() noexcept;

}  // namespace gecko::platform

#endif  // GECKO_PLATFORM_LINUX && GECKO_PLATFORM_LINUX_X11
