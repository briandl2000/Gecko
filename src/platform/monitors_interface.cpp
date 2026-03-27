#include "gecko/platform/monitors_interface.h"

#include "gecko/core/assert.h"
#include "gecko/core/services/log.h"
#include "private/labels.h"
#include "private/null_monitors_backend.h"

namespace gecko::platform {

Unique<IMonitorsBackend> CreateNullMonitorsBackend() noexcept;

Unique<IMonitorsBackend> IMonitorsBackend::Create(
    const PlatformConfig& config) noexcept
{
  // config.Backend is guaranteed to be a concrete value;
  // PlatformContext calls Resolve() before passing the config here.
  switch (config.Backend)
  {
  case DisplayBackendKind::Null:
    return CreateNullMonitorsBackend();

  case DisplayBackendKind::Xlib:
  case DisplayBackendKind::Xcb:
    // TODO: implement X11 monitor backend (XRandR)
    GECKO_WARN(labels::General,
               "Xlib monitor backend not yet implemented; using Null");
    return CreateNullMonitorsBackend();

  case DisplayBackendKind::Wayland:
    // TODO: implement Wayland monitor backend (wl_output)
    GECKO_WARN(labels::General,
               "Wayland monitor backend not yet implemented; using Null");
    return CreateNullMonitorsBackend();

  case DisplayBackendKind::Win32:
    // TODO: implement Win32 monitor backend (EnumDisplayMonitors)
    GECKO_WARN(labels::General,
               "Win32 monitor backend not yet implemented; using Null");
    return CreateNullMonitorsBackend();

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
