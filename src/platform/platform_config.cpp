#include "gecko/platform/platform_config.h"

#include "gecko/core/services/log.h"
#include "private/labels.h"

#include <cstdlib>

namespace gecko::platform {

namespace {

[[nodiscard]] bool IsXlibAvailable() noexcept
{
#if defined(GECKO_PLATFORM_LINUX) && defined(GECKO_PLATFORM_LINUX_X11)
  const char* d = ::std::getenv("DISPLAY");
  return d && d[0] != '\0';
#else
  return false;
#endif
}

[[nodiscard]] bool IsWaylandAvailable() noexcept
{
#if defined(GECKO_PLATFORM_LINUX) && defined(GECKO_PLATFORM_LINUX_WAYLAND)
  const char* d = ::std::getenv("WAYLAND_DISPLAY");
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

/// Returns true if the given concrete backend is available at runtime.
[[nodiscard]] bool IsBackendAvailable(DisplayBackendKind kind) noexcept
{
  switch (kind)
  {
  case DisplayBackendKind::Xlib:
  case DisplayBackendKind::Xcb:
    return IsXlibAvailable();
  case DisplayBackendKind::Wayland:
    return IsWaylandAvailable();
  case DisplayBackendKind::Win32:
#if defined(_WIN32)
    return true;
#else
    return false;
#endif
  case DisplayBackendKind::Null:
    return true;
  default:
    return false;
  }
}

/// Probe the runtime environment and return the best available concrete
/// backend.
[[nodiscard]] DisplayBackendKind ProbeBackend() noexcept
{
#if defined(_WIN32)
  return DisplayBackendKind::Win32;
#elif defined(GECKO_PLATFORM_LINUX)
  if (IsWaylandAvailable())
    return DisplayBackendKind::Wayland;
  if (IsXlibAvailable())
    return DisplayBackendKind::Xlib;
#endif

  return DisplayBackendKind::Null;
}

}  // namespace

PlatformConfig Resolve(const PlatformConfig& requested) noexcept
{
  PlatformConfig resolved = requested;

  if (resolved.Backend == DisplayBackendKind::Auto ||
      resolved.Backend == DisplayBackendKind::Unknown)
  {
    resolved.Backend = ProbeBackend();
    GECKO_INFO(labels::General, "Platform backend: Auto resolved to %s",
               BackendName(resolved.Backend));
    return resolved;
  }

  // Explicit backend requested — verify it's available.
  if (!IsBackendAvailable(resolved.Backend))
  {
    const DisplayBackendKind fallback = ProbeBackend();
    GECKO_WARN(labels::General,
               "Platform backend: %s requested but not available, "
               "falling back to %s",
               BackendName(resolved.Backend), BackendName(fallback));
    resolved.Backend = fallback;
    return resolved;
  }

  GECKO_INFO(labels::General, "Platform backend: %s (explicit)",
             BackendName(resolved.Backend));
  return resolved;
}

}  // namespace gecko::platform
