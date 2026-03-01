#include "gecko/core/utility/bit.h"

#include <catch2/catch_test_macros.hpp>

using namespace gecko;

enum class TestFlags : u8
{
  None = 0,
  Read = 1 << 0,
  Write = 1 << 1,
  Execute = 1 << 2,
};

TEST_CASE("Bit shift utility", "[core][bit]")
{
  REQUIRE(Bit(0) == 1);
  REQUIRE(Bit(1) == 2);
  REQUIRE(Bit(7) == 128);
  REQUIRE(Bit(63) == (1ull << 63));
  REQUIRE(Bit(64) == 0);
}

TEST_CASE("ToUnderlying converts enum to integer", "[core][bit]")
{
  REQUIRE(ToUnderlying(TestFlags::None) == 0);
  REQUIRE(ToUnderlying(TestFlags::Read) == 1);
  REQUIRE(ToUnderlying(TestFlags::Write) == 2);
  REQUIRE(ToUnderlying(TestFlags::Execute) == 4);
}

TEST_CASE("Enum bitwise OR", "[core][bit]")
{
  auto combined = TestFlags::Read | TestFlags::Write;
  REQUIRE(ToUnderlying(combined) == 3);
}

TEST_CASE("Enum bitwise AND", "[core][bit]")
{
  auto combined = TestFlags::Read | TestFlags::Write;
  REQUIRE(ToUnderlying(combined & TestFlags::Read) == 1);
  REQUIRE(ToUnderlying(combined & TestFlags::Execute) == 0);
}

TEST_CASE("Enum bitwise XOR", "[core][bit]")
{
  auto combined = TestFlags::Read | TestFlags::Write;
  auto toggled = combined ^ TestFlags::Read;
  REQUIRE(ToUnderlying(toggled) == 2);
}

TEST_CASE("Enum compound OR assignment", "[core][bit]")
{
  auto flags = TestFlags::None;
  flags |= TestFlags::Read;
  flags |= TestFlags::Execute;
  REQUIRE(ToUnderlying(flags) == 5);
}

TEST_CASE("Any checks non-zero", "[core][bit]")
{
  REQUIRE_FALSE(Any(TestFlags::None));
  REQUIRE(Any(TestFlags::Read));
  REQUIRE(Any(TestFlags::Read | TestFlags::Write));
}
