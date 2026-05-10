#pragma once

/// @file
/// Backend interface for monitor enumeration and property queries.
///
/// `IMonitorsBackend` is the lower-level abstraction over native
/// monitor APIs. Concrete backends live under `src/platform/<backend>/`
/// and are constructed via `IMonitorsBackend::Create`.

#include "gecko/core/api.h"
#include "gecko/core/ptr.h"
#include "gecko/core/services/events.h"
#include "gecko/platform/monitor.h"
#include "gecko/platform/platform_config.h"
#include "gecko/platform/platform_events.h"

namespace gecko::platform {

/// Backend abstraction over the native monitor enumeration API.
/// Methods are not thread-safe; call only from the platform/main thread.
class IMonitorsBackend
{
public:
  virtual ~IMonitorsBackend() = default;

  /// Construct the backend selected by `cfg`. Returns `nullptr` on failure.
  /// @param cfg  Platform configuration.
  [[nodiscard]]
  GECKO_API static Unique<IMonitorsBackend> Create(const PlatformConfig& cfg) noexcept;

  // Discovery -------------------------------------------------------

  /// Refresh the cached monitor list from the OS. Call after
  /// monitor-changed events to pick up new/removed monitors.
  GECKO_API virtual void EnumerateMonitors() noexcept = 0;
  /// Number of monitors currently known to the backend.
  GECKO_API virtual u32 GetMonitorCount() const noexcept = 0;

  /// Stable handle for the monitor at `index` (`0 <= index <
  /// GetMonitorCount()`).
  [[nodiscard]]
  GECKO_API virtual MonitorHandle GetMonitorHandle(u32 index) const noexcept = 0;

  // Properties ------------------------------------------------------

  /// Snapshot of `handle`'s properties. Returns a default-constructed
  /// `MonitorInfo` if `handle` is invalid.
  [[nodiscard]]
  GECKO_API virtual MonitorInfo GetMonitorProperties(MonitorHandle handle) const noexcept = 0;

  /// Handle of the system's primary monitor, or an invalid handle when
  /// no monitors are connected.
  [[nodiscard]]
  GECKO_API virtual MonitorHandle GetPrimaryMonitor() const noexcept = 0;

  /// Bounds + work area of `handle`. Cheaper than fetching the full
  /// `MonitorInfo` when only geometry is needed.
  [[nodiscard]]
  GECKO_API virtual MonitorBounds GetMonitorBounds(MonitorHandle handle) const noexcept = 0;

  // Event pump ------------------------------------------------------

  /// Process pending monitor-related OS events and emit them on the
  /// global event bus via `emitter`.
  /// @param emitter  Event-bus emitter representing the platform module.
  GECKO_API virtual void PumpEvents(const gecko::EventEmitter& emitter) noexcept = 0;
};

}  // namespace gecko::platform
