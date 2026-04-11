#include "gecko/platform/monitors_interface.h"

#include "gecko/core/assert.h"
#include "gecko/core/services/log.h"
#include "private/labels.h"
#include "private/null_monitors_backend.h"

namespace gecko::platform {

Unique<IMonitorsBackend> CreateNullMonitorsBackend() noexcept;

#if defined(GECKO_PLATFORM_LINUX) && defined(GECKO_PLATFORM_LINUX_X11)
Unique<IMonitorsBackend> CreateXlibMonitorsBackend() noexcept;
#endif

#if defined(GECKO_PLATFORM_LINUX) && defined(GECKO_PLATFORM_LINUX_WAYLAND)
Unique<IMonitorsBackend> CreateWaylandMonitorsBackend() noexcept;
#endif

#if defined(_WIN32)
Unique<IMonitorsBackend> CreateWin32MonitorsBackend() noexcept;
#endif

Unique<IMonitorsBackend> IMonitorsBackend::Create(
    const PlatformConfig& config) noexcept
{
  switch (config.Backend)
  {
  case DisplayBackendKind::Null:
    return CreateNullMonitorsBackend();

  case DisplayBackendKind::Xlib:
  case DisplayBackendKind::Xcb:
#if defined(GECKO_PLATFORM_LINUX) && defined(GECKO_PLATFORM_LINUX_X11)
    return CreateXlibMonitorsBackend();
#else
    GECKO_WARN(labels::General,
               "Xlib monitor backend not available in this build; using Null");
    return CreateNullMonitorsBackend();
#endif

  case DisplayBackendKind::Wayland:
#if defined(GECKO_PLATFORM_LINUX) && defined(GECKO_PLATFORM_LINUX_WAYLAND)
    return CreateWaylandMonitorsBackend();
#else
    GECKO_WARN(
        labels::General,
        "Wayland monitor backend not available in this build; using Null");
    return CreateNullMonitorsBackend();
#endif

  case DisplayBackendKind::Win32:
#if defined(_WIN32)
    return CreateWin32MonitorsBackend();
#else
    GECKO_WARN(labels::General,
               "Win32 monitor backend not available in this build; using Null");
    return CreateNullMonitorsBackend();
#endif

  case DisplayBackendKind::Cocoa:
    // TODO: implement Cocoa monitor backend (NSScreen)
    GECKO_WARN(labels::General,
               "Cocoa monitor backend not yet implemented; using Null");
    return CreateNullMonitorsBackend();

  default:
    GECKO_ASSERT(false && "Unresolved backend in IMonitorsBackend::Create");
    return CreateNullMonitorsBackend();
  }
}

}  // namespace gecko::platform
