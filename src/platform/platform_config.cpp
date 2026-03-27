#include "gecko/platform/platform_config.h"

#include "gecko/core/services/log.h"
#include "private/labels.h"

#include <cstdlib>

namespace gecko::platform {

namespace {

[[nodiscard]] bool IsXlibAvailable() noexcept
{
#if defined(GECKO_PLATFORM_LINUX) && defined(GECKO_PLATFORM_LINUX_X11)
  const char* d = std::getenv("DISPLAY");
  return d && d[0] != '\0';
#else
  return false;
#endif
}

[[nodiscard]] bool IsWaylandAvailable() noexcept
{
#if defined(GECKO_PLATFORM_LINUX) && defined(GECKO_PLATFORM_LINUX_WAYLAND)
  const char* d = std::getenv("WAYLAND_DISPLAY");
  return d && d[0] != '\0';
#else
  return false;
#endif
}

[[nodiscard]] const char* BackendName(DisplayBackendKind b) noexcept
{
  switch (b)
  {
  case DisplayBackendKind::Null:
    return "Null";
  case DisplayBackendKind::Win32:
    return "Win32";
  case DisplayBackendKind::Xlib:
    return "Xlib";
  case DisplayBackendKind::Xcb:
    return "Xcb";
  case DisplayBackendKind::Wayland:
    return "Wayland";
  case DisplayBackendKind::Cocoa:
    return "Cocoa";
  case DisplayBackendKind::Auto:
    return "Auto";
  default:
    return "Unknown";
  }
}

/// Probe the runtime environment and return the best available concrete
/// backend.
[[nodiscard]] DisplayBackendKind ProbeBackend() noexcept
{
  // Priority: Wayland > X11 > Null
  if (IsWaylandAvailable())
    return DisplayBackendKind::Wayland;
  if (IsXlibAvailable())
    return DisplayBackendKind::Xlib;

  return DisplayBackendKind::Null;
}

}  // namespace

PlatformConfig Resolve(const PlatformConfig& requested) noexcept
{
  PlatformConfig resolved = requested;

  if (resolved.Backend != DisplayBackendKind::Auto)
  {
    GECKO_INFO(labels::General, "Platform backend: %s (explicit)",
               BackendName(resolved.Backend));
    return resolved;
  }

  resolved.Backend = ProbeBackend();

  GECKO_INFO(labels::General, "Platform backend: Auto resolved to %s",
             BackendName(resolved.Backend));

  return resolved;
}

}  // namespace gecko::platform
