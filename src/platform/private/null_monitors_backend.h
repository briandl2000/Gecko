#pragma once

#include "gecko/core/services/events.h"
#include "gecko/platform/monitors_interface.h"

#include <vector>

namespace gecko::platform {

class NullMonitorsBackend final : public IMonitorsBackend
{
public:
  NullMonitorsBackend() noexcept;

  void EnumerateMonitors() noexcept override;
  u32 GetMonitorCount() const noexcept override;
  MonitorHandle GetMonitorHandle(u32 index) const noexcept override;
  MonitorInfo GetMonitorProperties(
      MonitorHandle handle) const noexcept override;
  MonitorHandle GetPrimaryMonitor() const noexcept override;
  MonitorBounds GetMonitorBounds(MonitorHandle handle) const noexcept override;

  /// No-op for the null backend: no OS monitor-change events.
  void PumpEvents(const gecko::EventEmitter& emitter) noexcept override;

private:
  struct MonitorEntry
  {
    MonitorHandle Handle {};
    MonitorInfo Info {};
  };

  std::vector<MonitorEntry> m_Monitors;
};

}  // namespace gecko::platform
