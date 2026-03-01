#include "gecko/core/utility/random.h"

#include <catch2/catch_test_macros.hpp>

using namespace gecko;

TEST_CASE("SeedRandom produces reproducible results", "[core][random]")
{
  SeedRandom(12345);
  u32 a = RandomU32(0, 1000);
  SeedRandom(12345);
  u32 b = RandomU32(0, 1000);
  REQUIRE(a == b);
}

TEST_CASE("RandomU32 respects range", "[core][random]")
{
  SeedRandom(42);
  for (int i = 0; i < 100; ++i)
  {
    u32 val = RandomU32(10, 20);
    REQUIRE(val >= 10);
    REQUIRE(val <= 20);
  }
}

TEST_CASE("RandomI32 respects range", "[core][random]")
{
  SeedRandom(42);
  for (int i = 0; i < 100; ++i)
  {
    i32 val = RandomI32(-50, 50);
    REQUIRE(val >= -50);
    REQUIRE(val <= 50);
  }
}

TEST_CASE("RandomF32 respects range", "[core][random]")
{
  SeedRandom(42);
  for (int i = 0; i < 100; ++i)
  {
    f32 val = RandomF32(0.0f, 1.0f);
    REQUIRE(val >= 0.0f);
    REQUIRE(val <= 1.0f);
  }
}

TEST_CASE("RandomF64 respects range", "[core][random]")
{
  SeedRandom(42);
  for (int i = 0; i < 100; ++i)
  {
    f64 val = RandomF64(-10.0, 10.0);
    REQUIRE(val >= -10.0);
    REQUIRE(val <= 10.0);
  }
}

TEST_CASE("RandomBool returns both values", "[core][random]")
{
  SeedRandom(42);
  bool sawTrue = false;
  bool sawFalse = false;
  for (int i = 0; i < 100; ++i)
  {
    if (RandomBool())
      sawTrue = true;
    else
      sawFalse = true;
  }
  REQUIRE(sawTrue);
  REQUIRE(sawFalse);
}

TEST_CASE("RandomBytes fills buffer", "[core][random]")
{
  SeedRandom(42);
  u8 buffer[32] = {};
  RandomBytes(buffer, sizeof(buffer));

  bool allZero = true;
  for (u8 b : buffer)
  {
    if (b != 0)
      allZero = false;
  }
  REQUIRE_FALSE(allZero);
}

TEST_CASE("random::Index stays in bounds", "[core][random]")
{
  SeedRandom(42);
  for (int i = 0; i < 100; ++i)
  {
    usize idx = random::Index(10);
    REQUIRE(idx < 10);
  }
  REQUIRE(random::Index(0) == 0);
}

TEST_CASE("random::Normalized returns [0, 1]", "[core][random]")
{
  SeedRandom(42);
  for (int i = 0; i < 100; ++i)
  {
    f32 val = random::Normalized();
    REQUIRE(val >= 0.0f);
    REQUIRE(val <= 1.0f);
  }
}

TEST_CASE("random::Signed returns [-1, 1]", "[core][random]")
{
  SeedRandom(42);
  for (int i = 0; i < 100; ++i)
  {
    f32 val = random::Signed();
    REQUIRE(val >= -1.0f);
    REQUIRE(val <= 1.0f);
  }
}
