#include "gecko/core/engine.h"
#include "gecko/core/services.h"
#include "gecko/runtime/event_bus.h"
#include "gecko/runtime/runtime_module.h"

#include <catch2/catch_test_macros.hpp>
#include <optional>

using namespace gecko;
using namespace gecko::runtime;

namespace {

struct TestServiceScope
{
  SystemAllocator alloc;
  NullJobSystem jobs;
  NullProfiler profiler;
  NullLogger logger;
  EventBus eventBus;
  CoreServicesModule runtimeMod;
  ::std::optional<::gecko::Engine> engine;

  TestServiceScope() : runtimeMod(jobs, profiler, logger, eventBus)
  {
    REQUIRE(SetAllocator(&alloc));
    engine = ::gecko::Engine::Create({&runtimeMod});
  }

  ~TestServiceScope()
  {
    engine.reset();
    ResetAllocator();
  }
};

constexpr EventCode TestEvent1 = MakeEvent(1, 1);
constexpr EventCode TestEvent2 = MakeEvent(1, 2);

struct PayloadData
{
  int Value;
};

EventView MakePayload(const PayloadData& d)
{
  return EventView {&d, static_cast<u32>(sizeof(d))};
}

}  // namespace

TEST_CASE("EventBus Init/Shutdown", "[runtime][events]")
{
  TestServiceScope scope;
}

TEST_CASE("EventBus subscribe and immediate delivery", "[runtime][events]")
{
  TestServiceScope scope;

  scope.eventBus.RegisterModule(1);
  EventEmitter emitter = scope.eventBus.CreateEmitter(1, 0);

  int received = 0;
  auto sub = scope.eventBus.Subscribe(
      TestEvent1,
      [](void* user, const EventMeta&, EventView) {
        auto* count = static_cast<int*>(user);
        *count += 1;
      },
      &received, {.delivery = SubscriptionDelivery::Immediate});

  PayloadData data {42};
  scope.eventBus.Send(emitter, TestEvent1, MakePayload(data));

  REQUIRE(received == 1);

  scope.eventBus.UnregisterModule(1);
}

TEST_CASE("EventBus queued events", "[runtime][events]")
{
  TestServiceScope scope;

  scope.eventBus.RegisterModule(1);
  EventEmitter emitter = scope.eventBus.CreateEmitter(1, 0);

  int received = 0;
  auto sub = scope.eventBus.Subscribe(
      TestEvent1,
      [](void* user, const EventMeta&, EventView) {
        auto* count = static_cast<int*>(user);
        *count += 1;
      },
      &received);

  PayloadData data {10};
  scope.eventBus.Send(emitter, TestEvent1, MakePayload(data));
  scope.eventBus.Send(emitter, TestEvent1, MakePayload(data));

  REQUIRE(received == 0);

  auto dispatched = scope.eventBus.Dispatch(100);
  REQUIRE(dispatched == 2);
  REQUIRE(received == 2);

  scope.eventBus.UnregisterModule(1);
}

TEST_CASE("EventBus unsubscribe via RAII", "[runtime][events]")
{
  TestServiceScope scope;

  scope.eventBus.RegisterModule(1);
  EventEmitter emitter = scope.eventBus.CreateEmitter(1, 0);

  int received = 0;
  {
    auto sub = scope.eventBus.Subscribe(
        TestEvent1,
        [](void* user, const EventMeta&, EventView) {
          *static_cast<int*>(user) += 1;
        },
        &received, {.delivery = SubscriptionDelivery::Immediate});

    PayloadData data {1};
    scope.eventBus.Send(emitter, TestEvent1, MakePayload(data));
    REQUIRE(received == 1);
  }

  PayloadData data {2};
  scope.eventBus.Send(emitter, TestEvent1, MakePayload(data));
  REQUIRE(received == 1);

  scope.eventBus.UnregisterModule(1);
}

TEST_CASE("EventBus multiple subscribers", "[runtime][events]")
{
  TestServiceScope scope;

  scope.eventBus.RegisterModule(1);
  EventEmitter emitter = scope.eventBus.CreateEmitter(1, 0);

  int count1 = 0;
  int count2 = 0;

  auto sub1 = scope.eventBus.Subscribe(
      TestEvent1,
      [](void* user, const EventMeta&, EventView) {
        *static_cast<int*>(user) += 1;
      },
      &count1, {.delivery = SubscriptionDelivery::Immediate});

  auto sub2 = scope.eventBus.Subscribe(
      TestEvent1,
      [](void* user, const EventMeta&, EventView) {
        *static_cast<int*>(user) += 1;
      },
      &count2, {.delivery = SubscriptionDelivery::Immediate});

  PayloadData data {1};
  scope.eventBus.Send(emitter, TestEvent1, MakePayload(data));

  REQUIRE(count1 == 1);
  REQUIRE(count2 == 1);

  scope.eventBus.UnregisterModule(1);
}

TEST_CASE("EventBus different event codes stay separate", "[runtime][events]")
{
  TestServiceScope scope;

  scope.eventBus.RegisterModule(1);
  EventEmitter emitter = scope.eventBus.CreateEmitter(1, 0);

  int count1 = 0;
  int count2 = 0;

  auto sub1 = scope.eventBus.Subscribe(
      TestEvent1,
      [](void* user, const EventMeta&, EventView) {
        *static_cast<int*>(user) += 1;
      },
      &count1, {.delivery = SubscriptionDelivery::Immediate});

  auto sub2 = scope.eventBus.Subscribe(
      TestEvent2,
      [](void* user, const EventMeta&, EventView) {
        *static_cast<int*>(user) += 1;
      },
      &count2, {.delivery = SubscriptionDelivery::Immediate});

  PayloadData data {1};
  scope.eventBus.Send(emitter, TestEvent1, MakePayload(data));

  REQUIRE(count1 == 1);
  REQUIRE(count2 == 0);

  scope.eventBus.UnregisterModule(1);
}

TEST_CASE("EventBus payload delivery", "[runtime][events]")
{
  TestServiceScope scope;

  scope.eventBus.RegisterModule(1);
  EventEmitter emitter = scope.eventBus.CreateEmitter(1, 0);

  int capturedValue = 0;
  auto sub = scope.eventBus.Subscribe(
      TestEvent1,
      [](void* user, const EventMeta&, EventView payload) {
        auto* data = static_cast<const PayloadData*>(payload.Data());
        *static_cast<int*>(user) = data->Value;
      },
      &capturedValue, {.delivery = SubscriptionDelivery::Immediate});

  PayloadData data {999};
  scope.eventBus.Send(emitter, TestEvent1, MakePayload(data));

  REQUIRE(capturedValue == 999);

  scope.eventBus.UnregisterModule(1);
}
