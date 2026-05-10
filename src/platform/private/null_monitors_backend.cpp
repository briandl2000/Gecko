#include "null_monitors_backend.h"

#include "gecko/core/ptr.h"
#include "gecko/core/services/log.h"
#include "labels.h"

namespace gecko::platform {

NullMonitorsBackend::NullMonitorsBackend() noexcept = default;

void NullMonitorsBackend::EnumerateMonitors() noexcept
{
  m_Monitors.clear();

  MonitorEntry entry;
  entry.Handle = MonitorHandle {1};

  MonitorInfo& info = entry.Info;
  info.SetName("Virtual Monitor");
  info.Bounds = math::Rect2D {0, 0, 1920, 1080};
  info.WorkArea = math::Rect2D {0, 0, 1920, 1040};
  info.RefreshRateMilliHz = 60000;
  info.Dpi = 96;
  info.DpiScale = 1.0F;
  info.MonitorColorSpace = ColorSpace::Srgb;
  info.IsPrimary = true;

  m_Monitors.push_back(entry);

  GECKO_INFO(labels::General, "NullMonitorsBackend: enumerated 1 virtual monitor");
}

u32 NullMonitorsBackend::GetMonitorCount() const noexcept
{
  return static_cast<u32>(m_Monitors.size());
}

MonitorHandle NullMonitorsBackend::GetMonitorHandle(u32 index) const noexcept
{
  if (index >= static_cast<u32>(m_Monitors.size()))
    return {};
  return m_Monitors[index].Handle;
}

MonitorInfo NullMonitorsBackend::GetMonitorProperties(MonitorHandle handle) const noexcept
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

MonitorHandle NullMonitorsBackend::GetPrimaryMonitor() const noexcept
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

MonitorBounds NullMonitorsBackend::GetMonitorBounds(MonitorHandle handle) const noexcept
{
  MonitorInfo info = GetMonitorProperties(handle);
  return {info.Bounds, info.WorkArea};
}

void NullMonitorsBackend::PumpEvents(const gecko::EventEmitter& /*emitter*/) noexcept
{
  // Null backend: no OS monitor-change events.
}

Unique<IMonitorsBackend> CreateNullMonitorsBackend() noexcept
{
  return CreateUnique<NullMonitorsBackend>();
}

}  // namespace gecko::platform
