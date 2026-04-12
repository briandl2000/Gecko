#pragma once

#include "gecko/core/api.h"
#include "gecko/core/ptr.h"
#include "gecko/core/services/events.h"
#include "gecko/platform/monitor.h"
#include "gecko/platform/platform_config.h"
#include "gecko/platform/platform_events.h"

namespace gecko::platform {

class IMonitorsBackend
{
public:
  virtual ~IMonitorsBackend() = default;

  [[nodiscard]]
  GECKO_API static Unique<IMonitorsBackend> Create(
      const PlatformConfig& cfg) noexcept;

  // ── Discovery ────────────────────────────────────────────────

  GECKO_API virtual void EnumerateMonitors() noexcept = 0;
  GECKO_API virtual u32 GetMonitorCount() const noexcept = 0;

  [[nodiscard]]
  GECKO_API virtual MonitorHandle GetMonitorHandle(
      u32 index) const noexcept = 0;

  // ── Properties ───────────────────────────────────────────────

  [[nodiscard]]
  GECKO_API virtual MonitorInfo GetMonitorProperties(
      MonitorHandle handle) const noexcept = 0;

  [[nodiscard]]
  GECKO_API virtual MonitorHandle GetPrimaryMonitor() const noexcept = 0;

  [[nodiscard]]
  GECKO_API virtual MonitorBounds GetMonitorBounds(
      MonitorHandle handle) const noexcept = 0;

  // ── Event pump ───────────────────────────────────────────────

  GECKO_API virtual void PumpEvents(
      const gecko::EventEmitter& emitter) noexcept = 0;
};

}  // namespace gecko::platform
