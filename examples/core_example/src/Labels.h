#pragma once

#include "gecko/gecko.h"

namespace gecko::examples::core_example {

namespace labels {
inline constexpr Label App = MakeLabel("example.core");
inline constexpr Label Main = MakeLabel("example.core.main");
inline constexpr Label Events = MakeLabel("example.core.events");
inline constexpr Label Worker = MakeLabel("example.core.worker");
inline constexpr Label Memory = MakeLabel("example.core.memory");
inline constexpr Label Compute = MakeLabel("example.core.compute");
}  // namespace labels

namespace events {
inline constexpr EventCode TestEvent = MakeEventCode(labels::App.Id, 1);

struct TestEventPayload
{
  u32 Value {0};
};
}  // namespace events

}  // namespace gecko::examples::core_example
