#include "gecko/platform/window.h"

#if defined(GECKO_PLATFORM_LINUX) && defined(GECKO_PLATFORM_LINUX_X11)

#include "../private/labels.h"
#include "../private/platform_utils.h"
#include "gecko/core/ptr.h"
#include "gecko/core/scope.h"
#include "gecko/core/services/events.h"
#include "gecko/core/services/log.h"
#include "gecko/platform/monitors_interface.h"
#include "gecko/platform/platform_events.h"

#include <cmath>
#include <cstring>
#include <vector>
#include <X11/extensions/Xrandr.h>
#include <X11/Xlib.h>

namespace gecko::platform {

namespace {

struct MonitorEntry
{
  MonitorHandle Handle {};
  MonitorInfo Info {};
  RROutput OutputId {0};
};

}  // namespace

class X11MonitorsBackend final : public IMonitorsBackend
{
public:
  X11MonitorsBackend() noexcept
  {
    m_Display = ::XOpenDisplay(nullptr);
    if (!m_Display)
    {
      GECKO_ERROR(labels::General,
                  "X11MonitorsBackend: failed to open display");
      return;
    }

    int eventBase = 0;
    int errorBase = 0;
    if (!::XRRQueryExtension(m_Display, &eventBase, &errorBase))
    {
      GECKO_WARN(labels::General,
                 "X11MonitorsBackend: XRandR extension not available");
      return;
    }
    m_RREventBase = eventBase;

    // Subscribe to screen change notifications for hotplug detection.
    ::XRRSelectInput(m_Display, DefaultRootWindow(m_Display),
                     RRScreenChangeNotifyMask | RROutputChangeNotifyMask |
                         RRCrtcChangeNotifyMask);

    GECKO_INFO(labels::General,
               "X11MonitorsBackend: initialized (display=%p, rrEventBase=%d)",
               m_Display, m_RREventBase);
  }

  ~X11MonitorsBackend() noexcept override
  {
    if (m_Display)
    {
      ::XCloseDisplay(m_Display);
      m_Display = nullptr;
    }
  }

  void EnumerateMonitors() noexcept override
  {
    GECKO_FUNC(labels::General);

    m_Monitors.clear();

    if (!m_Display)
      return;

    const int screen = DefaultScreen(m_Display);
    const ::Window root = RootWindow(m_Display, screen);

    XRRScreenResources* resources = ::XRRGetScreenResources(m_Display, root);
    if (!resources)
    {
      GECKO_WARN(labels::General,
                 "X11MonitorsBackend: XRRGetScreenResources failed");
      return;
    }

    const RROutput primaryOutput = ::XRRGetOutputPrimary(m_Display, root);

    for (int i = 0; i < resources->noutput; ++i)
    {
      XRROutputInfo* outInfo =
          ::XRRGetOutputInfo(m_Display, resources, resources->outputs[i]);
      if (!outInfo)
        continue;

      if (outInfo->connection != RR_Connected || outInfo->crtc == None)
      {
        ::XRRFreeOutputInfo(outInfo);
        continue;
      }

      XRRCrtcInfo* crtcInfo =
          ::XRRGetCrtcInfo(m_Display, resources, outInfo->crtc);
      if (!crtcInfo)
      {
        ::XRRFreeOutputInfo(outInfo);
        continue;
      }

      MonitorEntry entry;
      entry.OutputId = resources->outputs[i];
      entry.Handle = MonitorHandle {static_cast<u64>(resources->outputs[i])};

      MonitorInfo& info = entry.Info;
      info.SetName(outInfo->name);
      info.Bounds = math::Rect2D {static_cast<i32>(crtcInfo->x),
                                  static_cast<i32>(crtcInfo->y),
                                  static_cast<i32>(crtcInfo->width),
                                  static_cast<i32>(crtcInfo->height)};
      info.WorkArea = info.Bounds;
      info.IsPrimary = (resources->outputs[i] == primaryOutput);

      // Refresh rate from the current mode
      for (int m = 0; m < resources->nmode; ++m)
      {
        if (resources->modes[m].id == crtcInfo->mode)
        {
          const XRRModeInfo& mode = resources->modes[m];
          if (mode.hTotal > 0 && mode.vTotal > 0)
          {
            double rate = static_cast<double>(mode.dotClock) /
                          (static_cast<double>(mode.hTotal) *
                           static_cast<double>(mode.vTotal));
            info.RefreshRateMilliHz =
                static_cast<u32>(::std::round(rate * 1000.0));
          }
          break;
        }
      }

      // DPI from physical size
      if (outInfo->mm_width > 0 && outInfo->mm_height > 0)
      {
        double dpiX = static_cast<double>(crtcInfo->width) * 25.4 /
                      static_cast<double>(outInfo->mm_width);
        info.Dpi = static_cast<u32>(::std::round(dpiX));
        info.DpiScale = static_cast<float>(info.Dpi) / 96.0F;
      }

      m_Monitors.push_back(entry);

      GECKO_DEBUG(labels::General,
                  "Monitor: %s (%dx%d @ %d,%d, %u mHz, %u DPI%s)",
                  outInfo->name, crtcInfo->width, crtcInfo->height, crtcInfo->x,
                  crtcInfo->y, info.RefreshRateMilliHz, info.Dpi,
                  info.IsPrimary ? ", primary" : "");

      ::XRRFreeCrtcInfo(crtcInfo);
      ::XRRFreeOutputInfo(outInfo);
    }

    ::XRRFreeScreenResources(resources);

    GECKO_INFO(labels::General, "X11MonitorsBackend: enumerated %u monitor(s)",
               static_cast<u32>(m_Monitors.size()));
  }

  u32 GetMonitorCount() const noexcept override
  {
    return static_cast<u32>(m_Monitors.size());
  }

  bool GetMonitorHandle(u32 index,
                        MonitorHandle& outHandle) const noexcept override
  {
    if (index >= static_cast<u32>(m_Monitors.size()))
      return false;
    outHandle = m_Monitors[index].Handle;
    return true;
  }

  bool GetMonitorProperties(MonitorHandle handle,
                            MonitorInfo& outInfo) const noexcept override
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

  bool GetPrimaryMonitor(MonitorHandle& outHandle) const noexcept override
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

  bool GetMonitorBounds(MonitorHandle handle, math::Rect2D& outBounds,
                        math::Rect2D& outWorkArea) const noexcept override
  {
    MonitorInfo info;
    if (!GetMonitorProperties(handle, info))
      return false;
    outBounds = info.Bounds;
    outWorkArea = info.WorkArea;
    return true;
  }

  void PumpEvents(const gecko::EventEmitter& emitter) noexcept override
  {
    if (!m_Display || m_RREventBase < 0)
      return;

    while (::XPending(m_Display) > 0)
    {
      XEvent event;
      ::XNextEvent(m_Display, &event);

      if (event.type == m_RREventBase + RRScreenChangeNotify)
      {
        ::XRRUpdateConfiguration(&event);
        HandleScreenChange(emitter);
      }
      else if (event.type == m_RREventBase + RRNotify)
      {
        HandleScreenChange(emitter);
      }
    }
  }

private:
  void HandleScreenChange(const gecko::EventEmitter& emitter) noexcept
  {
    auto oldMonitors = m_Monitors;
    EnumerateMonitors();
    const u64 now = NowNsSafe();

    // Detect disconnected monitors
    for (const auto& old : oldMonitors)
    {
      bool found = false;
      for (const auto& cur : m_Monitors)
      {
        if (cur.OutputId == old.OutputId)
        {
          found = true;
          break;
        }
      }
      if (!found)
      {
        gecko::SendEvent(emitter, events::MonitorDisconnected,
                         events::MonitorDisconnectedPayload {old.Handle, now});
      }
    }

    // Detect connected or reconfigured monitors
    for (const auto& cur : m_Monitors)
    {
      bool wasPresent = false;
      for (const auto& old : oldMonitors)
      {
        if (old.OutputId == cur.OutputId)
        {
          wasPresent = true;
          // Check for property changes
          if (old.Info.Bounds != cur.Info.Bounds ||
              old.Info.RefreshRateMilliHz != cur.Info.RefreshRateMilliHz ||
              old.Info.Dpi != cur.Info.Dpi ||
              old.Info.IsPrimary != cur.Info.IsPrimary)
          {
            gecko::SendEvent(
                emitter, events::MonitorReconfigured,
                events::MonitorReconfiguredPayload {cur.Handle, now, cur.Info});
          }
          break;
        }
      }
      if (!wasPresent)
      {
        gecko::SendEvent(
            emitter, events::MonitorConnected,
            events::MonitorConnectedPayload {cur.Handle, now, cur.Info});
      }
    }
  }

  Display* m_Display {nullptr};
  int m_RREventBase {-1};
  std::vector<MonitorEntry> m_Monitors;
};

Unique<IMonitorsBackend> CreateXlibMonitorsBackend() noexcept
{
  return CreateUnique<X11MonitorsBackend>();
}

}  // namespace gecko::platform

#endif  // GECKO_PLATFORM_LINUX && GECKO_PLATFORM_LINUX_X11
