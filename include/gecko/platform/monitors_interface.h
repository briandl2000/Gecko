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

  GECKO_API virtual bool GetMonitorHandle(
      u32 index, MonitorHandle& outHandle) const noexcept = 0;

  // ── Properties ───────────────────────────────────────────────

  GECKO_API virtual bool GetMonitorProperties(
      MonitorHandle handle, MonitorInfo& outInfo) const noexcept = 0;

  GECKO_API virtual bool GetPrimaryMonitor(
      MonitorHandle& outHandle) const noexcept = 0;

  GECKO_API virtual bool GetMonitorBounds(
      MonitorHandle handle, math::Rect2D& outBounds,
      math::Rect2D& outWorkArea) const noexcept = 0;

  // ── Event pump ───────────────────────────────────────────────

  GECKO_API virtual void PumpEvents(
      const gecko::EventEmitter& emitter) noexcept = 0;
};

}  // namespace gecko::platform
