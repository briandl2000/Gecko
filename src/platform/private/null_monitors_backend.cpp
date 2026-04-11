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
  info.ColorSpace = ColorSpace::Srgb;
  info.IsPrimary = true;

  m_Monitors.push_back(entry);

  GECKO_INFO(labels::General,
             "NullMonitorsBackend: enumerated 1 virtual monitor");
}

u32 NullMonitorsBackend::GetMonitorCount() const noexcept
{
  return static_cast<u32>(m_Monitors.size());
}

bool NullMonitorsBackend::GetMonitorHandle(
    u32 index, MonitorHandle& outHandle) const noexcept
{
  if (index >= static_cast<u32>(m_Monitors.size()))
    return false;
  outHandle = m_Monitors[index].Handle;
  return true;
}

bool NullMonitorsBackend::GetMonitorProperties(
    MonitorHandle handle, MonitorInfo& outInfo) const noexcept
{
  if (!handle.IsValid())
    return false;
  for (const auto& entry : m_Monitors)
  {
    if (entry.Handle == handle)
    {
      outInfo = entry.Info;
      return true;
    }
  }
  return false;
}

bool NullMonitorsBackend::GetPrimaryMonitor(
    MonitorHandle& outHandle) const noexcept
{
  for (const auto& entry : m_Monitors)
  {
    if (entry.Info.IsPrimary)
    {
      outHandle = entry.Handle;
      return true;
    }
  }
  if (!m_Monitors.empty())
  {
    outHandle = m_Monitors[0].Handle;
    return true;
  }
  return false;
}

bool NullMonitorsBackend::GetMonitorBounds(
    MonitorHandle handle, math::Rect2D& outBounds,
    math::Rect2D& outWorkArea) const noexcept
{
  MonitorInfo info;
  if (!GetMonitorProperties(handle, info))
    return false;
  outBounds = info.Bounds;
  outWorkArea = info.WorkArea;
  return true;
}

void NullMonitorsBackend::PumpEvents(
    const gecko::EventEmitter& /*emitter*/) noexcept
{
  // Null backend: no OS monitor-change events.
}

Unique<IMonitorsBackend> CreateNullMonitorsBackend() noexcept
{
  return CreateUnique<NullMonitorsBackend>();
}

}  // namespace gecko::platform
