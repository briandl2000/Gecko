#pragma once

#include "gecko/core/api.h"
#include "gecko/core/types.h"

namespace gecko::platform {

enum class DisplayBackendKind : u8
{
  Unknown,
  Auto,
  Null,

  Win32,
  Xlib,
  Xcb,
  Wayland,
  Cocoa,
};

struct WindowConfig
{
  bool EnableHighDpi {true};
};

struct MonitorConfig
{
  bool EnableHotplugEvents {true};
};

struct PlatformConfig
{
  DisplayBackendKind Backend {DisplayBackendKind::Auto};
  WindowConfig Window;
  MonitorConfig Monitor;
};

// ──────────────────────────────────────────────────────────────
// Config resolution
// ──────────────────────────────────────────────────────────────

/// Resolve Auto/Unknown backends to a concrete backend for the
/// current platform.  PlatformContext calls this automatically.
GECKO_API PlatformConfig Resolve(const PlatformConfig& requested) noexcept;

}  // namespace gecko::platform
