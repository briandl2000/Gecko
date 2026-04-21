#include "gecko/core/utility/thread.h"

#include <catch2/catch_test_macros.hpp>

using namespace gecko;

TEST_CASE("HashThreadId returns non-zero", "[core][thread]")
{
  u32 id = HashThreadId();
  REQUIRE(id != 0);
}

TEST_CASE("HashThreadId is consistent within same thread", "[core][thread]")
{
  u32 a = HashThreadId();
  u32 b = HashThreadId();
  REQUIRE(a == b);
}

TEST_CASE("HardwareThreadCount returns at least 1", "[core][thread]")
{
  REQUIRE(HardwareThreadCount() >= 1);
}

TEST_CASE("SleepMs does not crash", "[core][thread]")
{
  SleepMs(1);
}

TEST_CASE("YieldThread does not crash", "[core][thread]")
{
  YieldThread();
}
