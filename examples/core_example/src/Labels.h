#pragma once

#include <gecko/core/labels.h>
#include <gecko/core/services/events.h>
#include <gecko/core/types.h>

namespace gecko::examples::core_example {

namespace labels {
inline constexpr ::gecko::Label App = ::gecko::MakeLabel("app.core_example");
inline constexpr ::gecko::Label Main = ::gecko::MakeLabel("app.core_example.main");
inline constexpr ::gecko::Label Events = ::gecko::MakeLabel("app.core_example.events");
inline constexpr ::gecko::Label Worker = ::gecko::MakeLabel("app.core_example.worker");
inline constexpr ::gecko::Label Memory = ::gecko::MakeLabel("app.core_example.memory");
inline constexpr ::gecko::Label Compute = ::gecko::MakeLabel("app.core_example.compute");
inline constexpr ::gecko::Label Simulation = ::gecko::MakeLabel("app.core_example.simulation");
}  // namespace labels

namespace events {
constexpr ::gecko::EventCode TestEvent = ::gecko::MakeEventCode(labels::App.Id, 0x0001);

struct TestEventPayload
{
  ::gecko::u32 value {0};
};
}  // namespace events

}  // namespace gecko::examples::core_example
