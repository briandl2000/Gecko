#pragma once

/// @file
/// Configuration values consumed by the platform module at startup.
///
/// `PlatformConfig` is the input to `PlatformModule::Startup`. Choose
/// `DisplayBackendKind::Auto` to let `Resolve` pick the best backend
/// for the current platform.

#include "gecko/core/api.h"
#include "gecko/core/types.h"

namespace gecko::platform {

/// Identifies a windowing/display backend.
enum class DisplayBackendKind : u8
{
  Unknown,  ///< Unset / unresolved.
  Auto,     ///< Pick a sensible default for the current platform.
  Null,     ///< Headless backend; no real windows or events.

  Win32,    ///< Windows: Win32 (`HWND`).
  Xlib,     ///< Linux: X11 via Xlib (`Display*` + `Window`).
  Xcb,      ///< Linux: X11 via XCB (`xcb_connection_t*` + `xcb_window_t`).
  Wayland,  ///< Linux: Wayland (`wl_display*` + `wl_surface*`).
  Cocoa,    ///< macOS: Cocoa (`NSWindow*`); not yet implemented.
};

/// Window-subsystem configuration.
struct WindowConfig
{
  bool EnableHighDpi {true};  ///< Opt into per-monitor DPI awareness.
};

/// Monitor-subsystem configuration.
struct MonitorConfig
{
  bool EnableHotplugEvents {true};  ///< Emit monitor add/remove events at runtime.
};

/// Top-level platform-module configuration.
struct PlatformConfig
{
  DisplayBackendKind Backend {DisplayBackendKind::Auto};  ///< Selected display backend.
  WindowConfig Window;                                    ///< Window-subsystem options.
  MonitorConfig Monitor;                                  ///< Monitor-subsystem options.
};

// Config resolution -------------------------------------------------

/// Resolve `Auto`/`Unknown` backends to a concrete backend for the
/// current platform. `PlatformContext` calls this automatically; call
/// it manually if you need to inspect the resolved configuration before
/// startup.
/// @param requested  User-supplied configuration.
/// @return The resolved configuration with a concrete `Backend`.
GECKO_API PlatformConfig Resolve(const PlatformConfig& requested) noexcept;

}  // namespace gecko::platform
