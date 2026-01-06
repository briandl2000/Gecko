#pragma once

#include "gecko/core/events.h"
#include "gecko/core/types.h"

namespace gecko::platform_events {

constexpr u8 kPlatformDomain = 0x01;

constexpr EventCode WindowResized = MakeEvent(kPlatformDomain, 0x0001);
constexpr EventCode WindowClosed = MakeEvent(kPlatformDomain, 0x0002);

struct WindowResizedPayload {
  u32 width{0};
  u32 height{0};
};

} // namespace gecko::platform_events
