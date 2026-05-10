#include "gecko/platform/windows_interface.h"

#include "gecko/core/assert.h"
#include "gecko/core/services/log.h"
#include "private/labels.h"
#include "private/null_windows_interface.h"

namespace gecko::platform {

Unique<IWindowsBackend> CreateNullWindowsBackend() noexcept;

#if defined(GECKO_PLATFORM_LINUX) && defined(GECKO_PLATFORM_LINUX_X11)
Unique<IWindowsBackend> CreateXlibWindowsBackend() noexcept;
#endif

#if defined(GECKO_PLATFORM_LINUX) && defined(GECKO_PLATFORM_LINUX_WAYLAND)
Unique<IWindowsBackend> CreateWaylandWindowsBackend() noexcept;
#endif

#if defined(GECKO_PLATFORM_WINDOWS)
Unique<IWindowsBackend> CreateWin32WindowsBackend() noexcept;
#endif

Unique<IWindowsBackend> IWindowsBackend::Create(const PlatformConfig& config) noexcept
{
  // config.Backend is guaranteed to be a concrete value;
  // PlatformContext calls Resolve() before passing the config here.
  switch (config.Backend)
  {
  case DisplayBackendKind::Null:
    return CreateUnique<NullWindowsBackend>();

  case DisplayBackendKind::Xlib:
  case DisplayBackendKind::Xcb:
#if defined(GECKO_PLATFORM_LINUX) && defined(GECKO_PLATFORM_LINUX_X11)
    return CreateXlibWindowsBackend();
#else
    GECKO_WARN(labels::General, "Xlib window backend not available in this build; using Null");
    return CreateUnique<NullWindowsBackend>();
#endif

  case DisplayBackendKind::Wayland:
#if defined(GECKO_PLATFORM_LINUX) && defined(GECKO_PLATFORM_LINUX_WAYLAND)
    return CreateWaylandWindowsBackend();
#else
    GECKO_WARN(labels::General, "Wayland window backend not available in this build; using Null");
    return CreateUnique<NullWindowsBackend>();
#endif

  case DisplayBackendKind::Win32:
#if defined(GECKO_PLATFORM_WINDOWS)
    return CreateWin32WindowsBackend();
#else
    GECKO_WARN(labels::General, "Win32 window backend not available in this build; using Null");
    return CreateUnique<NullWindowsBackend>();
#endif

  case DisplayBackendKind::Cocoa:
    GECKO_WARN(labels::General, "Cocoa window backend not available; using Null");
    return CreateUnique<NullWindowsBackend>();

  default:
    GECKO_ASSERT(false && "Unresolved backend in IWindowsBackend::Create");
    return CreateUnique<NullWindowsBackend>();
  }
}

}  // namespace gecko::platform
