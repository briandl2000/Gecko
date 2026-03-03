#pragma once

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

}  // namespace gecko::platform
