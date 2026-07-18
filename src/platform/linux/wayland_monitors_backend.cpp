#include "wayland_monitors_backend.h"

#if defined(GECKO_PLATFORM_LINUX) && defined(GECKO_PLATFORM_LINUX_WAYLAND)

#include "../private/labels.h"
#include "../private/platform_utils.h"
#include "gecko/core/ptr.h"
#include "gecko/core/format.h"
#include "gecko/core/scope.h"
#include "gecko/core/services/events.h"
#include "gecko/core/services/log.h"
#include "gecko/platform/platform_events.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iterator>

namespace gecko::platform {

namespace {
static void OutputGeometry(void* data, ::wl_output* /*output*/, i32 x, i32 y, i32 physW, i32 physH, i32 /*subpixel*/,
                           const char* make, const char* model, i32 /*transform*/)
{
  auto* entry = static_cast<WaylandMonitorEntry*>(data);
  entry->PendingX = x;
  entry->PendingY = y;
  entry->PendingPhysicalW = physW;
  entry->PendingPhysicalH = physH;

  // Build name from make + model
  char name[MaxMonitorNameLength] {};
  if (make && model)
  {
    FormatBuffer buffer {.Data = name, .Capacity = sizeof(name)};
    FormatTo(buffer, "{} {}", make, model);
  }
  else if (make)
    ::std::strncpy(name, make, sizeof(name) - 1);
  else if (model)
    ::std::strncpy(name, model, sizeof(name) - 1);
  entry->PendingName[sizeof(entry->PendingName) - 1] = '\0';
  FormatBuffer buffer {.Data = entry->PendingName, .Capacity = sizeof(entry->PendingName)};
  FormatTo(buffer, "{}", name);
}

static void OutputMode(void* data, ::wl_output* /*output*/, u32 flags, i32 width, i32 height, i32 refresh)
{
  if (!(flags & WL_OUTPUT_MODE_CURRENT))
    return;
  auto* entry = static_cast<WaylandMonitorEntry*>(data);
  entry->PendingModeW = width;
  entry->PendingModeH = height;
  entry->PendingRefreshMHz = static_cast<u32>(refresh);
}

static void OutputDone(void* data, ::wl_output* /*output*/)
{
  auto* entry = static_cast<WaylandMonitorEntry*>(data);
  entry->Done = true;

  MonitorInfo& info = entry->Info;
  info.SetName(entry->PendingName);
  info.Bounds = math::Rect2D {entry->PendingX, entry->PendingY, entry->PendingModeW, entry->PendingModeH};
  info.WorkArea = info.Bounds;
  info.RefreshRateMilliHz = entry->PendingRefreshMHz;

  if (entry->PendingPhysicalW > 0 && entry->PendingModeW > 0)
  {
    double dpi = static_cast<double>(entry->PendingModeW) * 25.4 / static_cast<double>(entry->PendingPhysicalW);
    info.Dpi = static_cast<u32>(::std::round(dpi));
    info.DpiScale = static_cast<float>(info.Dpi) / 96.0F;
  }
  else
  {
    info.Dpi = 96 * static_cast<u32>(entry->PendingScale);
    info.DpiScale = static_cast<float>(entry->PendingScale);
  }
}

static void OutputScale(void* data, ::wl_output* /*output*/, i32 factor)
{
  auto* entry = static_cast<WaylandMonitorEntry*>(data);
  entry->PendingScale = factor;
}

static void OutputName(void* data, ::wl_output* /*output*/, const char* name)
{
  auto* entry = static_cast<WaylandMonitorEntry*>(data);
  if (name)
    ::std::strncpy(entry->PendingName, name, MaxMonitorNameLength - 1);
}

static void OutputDescription(void* /*data*/, ::wl_output* /*output*/, const char* /*description*/)
{
  // Not used -- we prefer the short name.
}

static constexpr ::wl_output_listener OutputListener = {
    OutputGeometry, OutputMode, OutputDone, OutputScale, OutputName, OutputDescription,
};

}  // namespace

WaylandMonitorsBackend::WaylandMonitorsBackend() noexcept
{
  m_Display = ::wl_display_connect(nullptr);
  if (!m_Display)
  {
    GECKO_ERROR(labels::General, "WaylandMonitorsBackend: failed to connect to display");
    return;
  }

  m_Registry = ::wl_display_get_registry(m_Display);
  ::wl_registry_add_listener(m_Registry, &s_RegistryListener, this);

  // Initial roundtrip to discover globals.
  ::wl_display_roundtrip(m_Display);

  GECKO_INFO(labels::General, "WaylandMonitorsBackend: initialized (display={})", m_Display);
}

WaylandMonitorsBackend::~WaylandMonitorsBackend() noexcept
{
  for (auto& entry : m_Monitors)
  {
    if (entry.Output)
      ::wl_output_destroy(entry.Output);
  }
  m_Monitors.clear();

  if (m_Registry)
    ::wl_registry_destroy(m_Registry);
  if (m_Display)
    ::wl_display_disconnect(m_Display);
}

void WaylandMonitorsBackend::EnumerateMonitors() noexcept
{
  GECKO_SCOPE(labels::General);

  if (!m_Display)
    return;

  // Second roundtrip to ensure all ::wl_output events (including done) arrive
  ::wl_display_roundtrip(m_Display);

  // Mark first monitor as primary (Wayland has no primary concept)
  if (!m_Monitors.empty())
    m_Monitors.front().Info.IsPrimary = true;

  GECKO_INFO(labels::General, "WaylandMonitorsBackend: enumerated {} monitor(s)", static_cast<u32>(m_Monitors.size()));
}

u32 WaylandMonitorsBackend::GetMonitorCount() const noexcept
{
  return static_cast<u32>(m_Monitors.size());
}

MonitorHandle WaylandMonitorsBackend::GetMonitorHandle(u32 index) const noexcept
{
  if (index >= static_cast<u32>(m_Monitors.size()))
    return {};
  auto it = m_Monitors.begin();
  ::std::advance(it, index);
  return it->Handle;
}

MonitorInfo WaylandMonitorsBackend::GetMonitorProperties(MonitorHandle handle) const noexcept
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

MonitorHandle WaylandMonitorsBackend::GetPrimaryMonitor() const noexcept
{
  for (const auto& entry : m_Monitors)
  {
    if (entry.Info.IsPrimary)
      return entry.Handle;
  }
  if (!m_Monitors.empty())
    return m_Monitors.front().Handle;
  return {};
}

MonitorBounds WaylandMonitorsBackend::GetMonitorBounds(MonitorHandle handle) const noexcept
{
  MonitorInfo info = GetMonitorProperties(handle);
  return {info.Bounds, info.WorkArea};
}

void WaylandMonitorsBackend::PumpEvents(const gecko::EventEmitter& emitter) noexcept
{
  if (!m_Display)
    return;

  // Non-blocking dispatch of pending events.
  ::wl_display_dispatch_pending(m_Display);
  ::wl_display_flush(m_Display);

  // Check for newly added/removed monitors since last pump.
  if (m_Dirty)
  {
    m_Dirty = false;
    EmitChanges(emitter);
  }
}

void WaylandMonitorsBackend::HandleGlobal(::wl_registry* registry, u32 name, const char* interface) noexcept
{
  if (::std::strcmp(interface, wl_output_interface.name) != 0)
    return;

  auto* output = static_cast<::wl_output*>(::wl_registry_bind(registry, name, &wl_output_interface, 4));
  if (!output)
    return;

  WaylandMonitorEntry entry;
  entry.Output = output;
  entry.GlobalName = name;
  entry.Handle = MonitorHandle {static_cast<u64>(name) + 1};

  m_Monitors.push_back(entry);

  // Listener data points into the list -- stable through insert and erase.
  ::wl_output_add_listener(output, &OutputListener, &m_Monitors.back());

  m_Dirty = true;
}

void WaylandMonitorsBackend::HandleGlobalRemove(u32 name) noexcept
{
  auto it = ::std::find_if(m_Monitors.begin(), m_Monitors.end(),
                           [name](const WaylandMonitorEntry& e) { return e.GlobalName == name; });
  if (it != m_Monitors.end())
  {
    m_RemovedHandles.push_back(it->Handle);
    if (it->Output)
      ::wl_output_destroy(it->Output);
    m_Monitors.erase(it);
    m_Dirty = true;
  }
}

void WaylandMonitorsBackend::EmitChanges(const gecko::EventEmitter& emitter) noexcept
{
  const u64 now = NowNsSafe();

  for (const auto& handle : m_RemovedHandles)
  {
    gecko::SendEvent(emitter, events::MonitorDisconnected, events::MonitorDisconnectedPayload {handle, now});
  }
  m_RemovedHandles.clear();

  for (auto& entry : m_Monitors)
  {
    if (!entry.Done)
      continue;

    if (!entry.Announced)
    {
      gecko::SendEvent(emitter, events::MonitorConnected,
                       events::MonitorConnectedPayload {entry.Handle, now, entry.Info});
      entry.Announced = true;
      entry.LastInfo = entry.Info;
    }
    else if (entry.Info.Bounds != entry.LastInfo.Bounds ||
             entry.Info.RefreshRateMilliHz != entry.LastInfo.RefreshRateMilliHz ||
             entry.Info.Dpi != entry.LastInfo.Dpi || entry.Info.IsPrimary != entry.LastInfo.IsPrimary)
    {
      gecko::SendEvent(emitter, events::MonitorReconfigured,
                       events::MonitorReconfiguredPayload {entry.Handle, now, entry.Info});
      entry.LastInfo = entry.Info;
    }
  }
}

void WaylandMonitorsBackend::RegistryGlobal(void* data, ::wl_registry* registry, u32 name, const char* interface,
                                            u32 /*version*/)
{
  auto* self = static_cast<WaylandMonitorsBackend*>(data);
  self->HandleGlobal(registry, name, interface);
}

void WaylandMonitorsBackend::RegistryGlobalRemove(void* data, ::wl_registry* /*registry*/, u32 name)
{
  auto* self = static_cast<WaylandMonitorsBackend*>(data);
  self->HandleGlobalRemove(name);
}

Unique<IMonitorsBackend> CreateWaylandMonitorsBackend() noexcept
{
  return CreateUnique<WaylandMonitorsBackend>();
}

}  // namespace gecko::platform

#endif  // GECKO_PLATFORM_LINUX && GECKO_PLATFORM_LINUX_WAYLAND
