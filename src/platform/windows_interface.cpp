#include "gecko/platform/windows_interface.h"

#include "gecko/core/services/log.h"
#include "null_windows_interface.h"
#include "private/labels.h"

#include <cstdlib>

namespace gecko::platform {

Unique<IWindowsBackend> CreateNullWindowsBackend() noexcept;

#if defined(GECKO_PLATFORM_LINUX) && defined(GECKO_PLATFORM_LINUX_X11)
Unique<IWindowsBackend> CreateXlibWindowsBackend() noexcept;
#endif

// #if defined(GECKO_PLATFORM_LINUX) && defined(GECKO_PLATFORM_LINUX_WAYLAND)
// Unique<IWindowsBackend> CreateWaylandWindowsBackend() noexcept;
// #endif

namespace {

Unique<IWindowsBackend> TryCreateXlibBackend() noexcept
{
#if defined(GECKO_PLATFORM_LINUX) && defined(GECKO_PLATFORM_LINUX_X11)
  const char* display = std::getenv("DISPLAY");
  if (display && display[0] != '\0')
    return CreateXlibWindowsBackend();
#endif
  return nullptr;
}

Unique<IWindowsBackend> TryCreateWaylandBackend() noexcept
{
  // #if defined(GECKO_PLATFORM_LINUX) && defined(GECKO_PLATFORM_LINUX_WAYLAND)
  //   const char* display = std::getenv("WAYLAND_DISPLAY");
  //   if (display && display[0] != '\0')
  //     return CreateWaylandWindowsBackend();
  // #endif
  return nullptr;
}

}  // namespace

Unique<IWindowsBackend> IWindowsBackend::Create(
    const PlatformConfig& config) noexcept
{
  switch (config.Backend)
  {
  case DisplayBackendKind::Null:
    return CreateUnique<NullWindowsBackend>();

  case DisplayBackendKind::Xlib:
    if (auto backend = TryCreateXlibBackend())
      return backend;
    GECKO_WARN(labels::General, "Requested Xlib window backend, but it's "
                                "not available; using Null backend\n");
    return CreateNullWindowsBackend();

  case DisplayBackendKind::Wayland:
    if (auto backend = TryCreateWaylandBackend())
      return backend;
    GECKO_WARN(labels::General, "Requested Wayland backend, but it's "
                                "not available; using Null backend\n");
    return CreateNullWindowsBackend();

  case DisplayBackendKind::Win32:
    GECKO_WARN(labels::General, "Requested Win32 backend, not implemented yet; "
                                "using Null backend\n");
    return CreateNullWindowsBackend();

  case DisplayBackendKind::Cocoa:
    GECKO_WARN(labels::General, "Requested Cocoa backend, not implemented yet; "
                                "using Null backend\n");
    return CreateNullWindowsBackend();

  case DisplayBackendKind::Auto:
  default:
    if (auto backend = TryCreateWaylandBackend())
      return backend;
    if (auto backend = TryCreateXlibBackend())
      return backend;
    return CreateNullWindowsBackend();
  }
}

}  // namespace gecko::platform
