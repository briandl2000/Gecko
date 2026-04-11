#include "gecko/core/services/events.h"

#include <catch2/catch_test_macros.hpp>

using namespace gecko;

TEST_CASE("MakeEventCode and extraction", "[core][events]")
{
  u64 moduleId = 0x00000005'00000000ull;
  u32 localCode = 42;

  EventCode code = MakeEventCode(moduleId, localCode);
  REQUIRE(GetEventModule(code) == 5);
  REQUIRE(GetEventLocal(code) == 42);
}

TEST_CASE("MakeEvent domain/local encoding", "[core][events]")
{
  EventCode code = MakeEvent(3, 100);
  REQUIRE(EventDomain(code) == 3);
  REQUIRE(EventLocal(code) == 100);
}

TEST_CASE("EventView default construction", "[core][events]")
{
  EventView view;
  REQUIRE(view.ptr == nullptr);
  REQUIRE(view.size == 0);
  REQUIRE(view.Data() == nullptr);
}

TEST_CASE("EventView with data", "[core][events]")
{
  int data = 42;
  EventView view {&data, sizeof(data)};
  REQUIRE(view.Data() != nullptr);
  REQUIRE(view.size == sizeof(int));
}

TEST_CASE("EventSubscription move semantics", "[core][events]")
{
  EventSubscription a;
  EventSubscription b = ::std::move(a);
  // No crash — move from default state is safe
}

TEST_CASE("NullEventBus operations don't crash", "[core][events]")
{
  NullEventBus bus;
  REQUIRE(bus.Init());

  EventEmitter emitter = bus.CreateEmitter(1, 0);

  int payload = 42;
  bus.Send(emitter, MakeEvent(1, 1), EventView {&payload, sizeof(payload)});

  REQUIRE(bus.Dispatch(100) == 0);

  bus.Shutdown();
}

TEST_CASE("EventLocal masks to 24 bits", "[core][events]")
{
  EventCode code = MakeEvent(0, 0x00FF'FFFF);
  REQUIRE(EventLocal(code) == 0x00FF'FFFF);

  EventCode overflow = MakeEvent(0, 0x01FF'FFFF);
  REQUIRE(EventLocal(overflow) == 0x00FF'FFFF);
}
