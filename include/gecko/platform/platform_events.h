#pragma once

#include "gecko/core/labels.h"
#include "gecko/core/services/events.h"
#include "gecko/core/types.h"
#include "gecko/platform/monitor.h"
#include "gecko/platform/window.h"

namespace gecko::platform::events {

// ──────────────────────────────────────────────────────────────────────────
// Event codes
//
// All platform events are keyed to the "gecko.platform" module so that the
// event bus can validate emitter/code consistency at runtime.
// Window events:  local 0x0001 – 0x00FF
// Monitor events: local 0x0100 – 0x01FF
// ──────────────────────────────────────────────────────────────────────────

namespace detail {
inline constexpr u64 PlatformModuleId = ::gecko::MakeLabel("gecko.platform").Id;
}

// Window
inline constexpr gecko::EventCode WindowClosed =
    gecko::MakeEventCode(detail::PlatformModuleId, 0x0001);
inline constexpr gecko::EventCode WindowCloseRequested =
    gecko::MakeEventCode(detail::PlatformModuleId, 0x0002);
inline constexpr gecko::EventCode WindowResized =
    gecko::MakeEventCode(detail::PlatformModuleId, 0x0003);
inline constexpr gecko::EventCode WindowDpiChanged =
    gecko::MakeEventCode(detail::PlatformModuleId, 0x0004);
inline constexpr gecko::EventCode WindowKey =
    gecko::MakeEventCode(detail::PlatformModuleId, 0x0005);
inline constexpr gecko::EventCode WindowChar =
    gecko::MakeEventCode(detail::PlatformModuleId, 0x0006);
inline constexpr gecko::EventCode WindowMouseMove =
    gecko::MakeEventCode(detail::PlatformModuleId, 0x0007);
inline constexpr gecko::EventCode WindowMouseButton =
    gecko::MakeEventCode(detail::PlatformModuleId, 0x0008);
inline constexpr gecko::EventCode WindowMouseWheel =
    gecko::MakeEventCode(detail::PlatformModuleId, 0x0009);

// Monitor
inline constexpr gecko::EventCode MonitorConnected =
    gecko::MakeEventCode(detail::PlatformModuleId, 0x0100);
inline constexpr gecko::EventCode MonitorDisconnected =
    gecko::MakeEventCode(detail::PlatformModuleId, 0x0101);
inline constexpr gecko::EventCode MonitorReconfigured =
    gecko::MakeEventCode(detail::PlatformModuleId, 0x0102);

// ──────────────────────────────────────────────────────────────────────────
// Window event payloads
//
// Each event is a flat, copyable struct — no unions.
// Cast EventView::Data() to the matching type in a subscriber.
// ──────────────────────────────────────────────────────────────────────────

struct WindowClosedPayload
{
  WindowHandle Window;
  u64 TimeNs;
};

struct WindowCloseRequestedPayload
{
  WindowHandle Window;
  u64 TimeNs;
};

struct WindowResizedPayload
{
  WindowHandle Window;
  u64 TimeNs;
  u32 Width;
  u32 Height;
};

struct WindowDpiChangedPayload
{
  WindowHandle Window;
  u64 TimeNs;
  u32 Dpi;
  float Scale;
};

struct WindowKeyPayload
{
  WindowHandle Window;
  u64 TimeNs;
  u32 Key;
  u8 Down;
  u8 Repeat;
};

struct WindowCharPayload
{
  WindowHandle Window;
  u64 TimeNs;
  u32 Codepoint;
};

struct WindowMouseMovePayload
{
  WindowHandle Window;
  u64 TimeNs;
  i32 X;
  i32 Y;
};

struct WindowMouseButtonPayload
{
  WindowHandle Window;
  u64 TimeNs;
  u8 Button;
  u8 Down;
};

struct WindowMouseWheelPayload
{
  WindowHandle Window;
  u64 TimeNs;
  float DeltaX;
  float DeltaY;
};

// ──────────────────────────────────────────────────────────────────────────
// Monitor event payloads
// ──────────────────────────────────────────────────────────────────────────

struct MonitorConnectedPayload
{
  MonitorHandle Monitor;
  u64 TimeNs;
  MonitorInfo Info;
};

struct MonitorDisconnectedPayload
{
  MonitorHandle Monitor;
  u64 TimeNs;
};

struct MonitorReconfiguredPayload
{
  MonitorHandle Monitor;
  u64 TimeNs;
  MonitorInfo NewProperties;
};

}  // namespace gecko::platform::events
