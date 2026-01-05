#pragma once

#include "gecko/core/events.h"
#include "gecko/core/types.h"

namespace gecko::platform_events {

constexpr u8 kDomain = 0x01;

constexpr EventCode WindowResized = MakeEvent(kDomain, 0x0000);
constexpr EventCode WindowClosed = MakeEvent(kDomain, 0x0001);

struct WindowResizedPayload {
  u32 width{0};
  u32 height{0};
};

} // namespace gecko::platform_events
