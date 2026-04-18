#pragma once

#if defined(GECKO_PLATFORM_LINUX) && defined(GECKO_PLATFORM_LINUX_WAYLAND)

#include "gecko/platform/monitors_interface.h"

#include <deque>
#include <vector>
#include <wayland-client.h>

namespace gecko::platform {

struct WaylandMonitorEntry
{
  MonitorHandle Handle {};
  MonitorInfo Info {};
  wl_output* Output {nullptr};
  u32 GlobalName {0};
  bool Done {false};
  bool Announced {false};

  // Snapshot of last-emitted state for reconfigured detection.
  MonitorInfo LastInfo {};

  // Pending state accumulated from wl_output events before "done".
  i32 PendingX {0};
  i32 PendingY {0};
  i32 PendingPhysicalW {0};
  i32 PendingPhysicalH {0};
  i32 PendingModeW {0};
  i32 PendingModeH {0};
  u32 PendingRefreshMHz {0};
  i32 PendingScale {1};
  char PendingName[MaxMonitorNameLength] {};
};

class WaylandMonitorsBackend final : public IMonitorsBackend
{
public:
  WaylandMonitorsBackend() noexcept;
  ~WaylandMonitorsBackend() noexcept override;

  void EnumerateMonitors() noexcept override;
  u32 GetMonitorCount() const noexcept override;
  MonitorHandle GetMonitorHandle(u32 index) const noexcept override;
  MonitorInfo GetMonitorProperties(MonitorHandle handle) const noexcept override;
  MonitorHandle GetPrimaryMonitor() const noexcept override;
  MonitorBounds GetMonitorBounds(MonitorHandle handle) const noexcept override;
  void PumpEvents(const gecko::EventEmitter& emitter) noexcept override;

private:
  void HandleGlobal(wl_registry* registry, u32 name,
                    const char* interface) noexcept;
  void HandleGlobalRemove(u32 name) noexcept;
  void EmitChanges(const gecko::EventEmitter& emitter) noexcept;

  static void RegistryGlobal(void* data, wl_registry* registry, u32 name,
                             const char* interface, u32 version);
  static void RegistryGlobalRemove(void* data, wl_registry* registry,
                                   u32 name);

  static constexpr wl_registry_listener s_RegistryListener = {
      RegistryGlobal,
      RegistryGlobalRemove,
  };

  wl_display* m_Display {nullptr};
  wl_registry* m_Registry {nullptr};
  std::deque<WaylandMonitorEntry> m_Monitors;
  std::vector<MonitorHandle> m_RemovedHandles;
  bool m_Dirty {false};
};

Unique<IMonitorsBackend> CreateWaylandMonitorsBackend() noexcept;

}  // namespace gecko::platform

#endif  // GECKO_PLATFORM_LINUX && GECKO_PLATFORM_LINUX_WAYLAND
