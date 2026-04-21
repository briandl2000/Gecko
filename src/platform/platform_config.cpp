#include "gecko/platform/platform_config.h"

#include "gecko/core/services/log.h"
#include "private/labels.h"

#include <cstdlib>

#if defined(GECKO_PLATFORM_LINUX) && defined(GECKO_PLATFORM_LINUX_X11)
#include <X11/Xlib.h>
#endif

#if defined(GECKO_PLATFORM_LINUX) && defined(GECKO_PLATFORM_LINUX_WAYLAND)
#include <wayland-client.h>
#endif

namespace gecko::platform {

namespace {

[[nodiscard]] bool IsXlibAvailable() noexcept
{
#if defined(GECKO_PLATFORM_LINUX) && defined(GECKO_PLATFORM_LINUX_X11)
  // Try the DISPLAY env var first, then fall back to ":0" (covers SSH sessions
  // where the desktop environment variables aren't inherited).
  ::Display* dpy = ::XOpenDisplay(nullptr);
  if (!dpy)
  {
    dpy = ::XOpenDisplay(":0");
    if (dpy)
    {
      // Propagate to the process environment so all subsequent
      // XOpenDisplay(NULL) calls in the backend code pick it up automatically.
      ::setenv("DISPLAY", ":0", 0);
    }
  }
  if (dpy)
  {
    ::XCloseDisplay(dpy);
    return true;
  }
  GECKO_WARN(labels::Platform,
             "X11: XOpenDisplay failed – DISPLAY not set or X server "
             "unreachable");
  return false;
#else
  return false;
#endif
}

[[nodiscard]] bool IsWaylandAvailable() noexcept
{
#if defined(GECKO_PLATFORM_LINUX) && defined(GECKO_PLATFORM_LINUX_WAYLAND)
  struct ::wl_display* dpy = ::wl_display_connect(nullptr);
  if (dpy)
  {
    ::wl_display_disconnect(dpy);
    return true;
  }
  GECKO_WARN(labels::Platform,
             "Wayland: wl_display_connect failed – WAYLAND_DISPLAY not set or "
             "compositor unreachable");
  return false;
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
#if defined(GECKO_PLATFORM_WINDOWS)
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
#if defined(GECKO_PLATFORM_WINDOWS)
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
