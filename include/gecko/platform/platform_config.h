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
{};

struct MonitorConfig
{};

struct PlatformConfig
{
  DisplayBackendKind Backend {DisplayBackendKind::Auto};
  WindowConfig Window;
  MonitorConfig Monitor;
};

// ──────────────────────────────────────────────────────────────
// Config resolution
// ──────────────────────────────────────────────────────────────

/// Resolve a PlatformConfig into a fully concrete configuration.
///
/// Handles Auto backend detection: probes the runtime environment
/// (WAYLAND_DISPLAY, DISPLAY, etc.) and compile-time availability
/// to pick the best concrete backend.  The returned config always
/// has Backend != Auto and != Unknown, so downstream factories can
/// assume they receive a verified, actionable value.
///
/// Call this once before constructing backends; PlatformContext
/// does this automatically.
GECKO_API PlatformConfig Resolve(const PlatformConfig& requested) noexcept;

}  // namespace gecko::platform
