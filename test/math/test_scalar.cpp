#include "gecko/math/scalar.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace gecko;
using namespace gecko::math;
using Catch::Matchers::WithinAbs;

TEST_CASE("Math constants are correct", "[scalar][constants]")
{
  REQUIRE_THAT(Pi, WithinAbs(3.14159265f, 0.00001f));
  REQUIRE_THAT(TwoPi, WithinAbs(6.28318530f, 0.00001f));
  REQUIRE_THAT(HalfPi, WithinAbs(1.57079632f, 0.00001f));
}

TEST_CASE("Angle conversions", "[scalar][angles]")
{
  REQUIRE_THAT(ToRadians(180.0f), WithinAbs(Pi, 0.00001f));
  REQUIRE_THAT(ToRadians(90.0f), WithinAbs(HalfPi, 0.00001f));
  REQUIRE_THAT(ToDegrees(Pi), WithinAbs(180.0f, 0.00001f));
  REQUIRE_THAT(ToDegrees(HalfPi), WithinAbs(90.0f, 0.00001f));
}

TEST_CASE("Scalar utility functions", "[scalar][utils]")
{
  SECTION("Min and Max")
  {
    REQUIRE(Min(5.0f, 3.0f) == 3.0f);
    REQUIRE(Max(5.0f, 3.0f) == 5.0f);
  }

  SECTION("Clamp")
  {
    REQUIRE(Clamp(5.0f, 0.0f, 10.0f) == 5.0f);
    REQUIRE(Clamp(-5.0f, 0.0f, 10.0f) == 0.0f);
    REQUIRE(Clamp(15.0f, 0.0f, 10.0f) == 10.0f);
    REQUIRE(Saturate(-1.0f) == 0.0f);
    REQUIRE(Saturate(2.0f) == 1.0f);
  }

  SECTION("Lerp")
  {
    REQUIRE_THAT(Lerp(0.0f, 10.0f, 0.5f), WithinAbs(5.0f, 0.00001f));
    REQUIRE_THAT(Lerp(0.0f, 10.0f, 0.0f), WithinAbs(0.0f, 0.00001f));
    REQUIRE_THAT(Lerp(0.0f, 10.0f, 1.0f), WithinAbs(10.0f, 0.00001f));
  }

  SECTION("Smoothstep")
  {
    REQUIRE_THAT(Smoothstep(0.0f, 1.0f, 0.5f), WithinAbs(0.5f, 0.00001f));
    REQUIRE_THAT(Smoothstep(0.0f, 1.0f, 0.0f), WithinAbs(0.0f, 0.00001f));
    REQUIRE_THAT(Smoothstep(0.0f, 1.0f, 1.0f), WithinAbs(1.0f, 0.00001f));
  }

  SECTION("Approximation and rounding")
  {
    REQUIRE(IsNearlyEqual(1.0f, 1.0f + Epsilon * 0.5f));
    REQUIRE(IsNearlyZero(Epsilon * 0.5f));
    REQUIRE(Sign(-5.0f) == -1.0f);
    REQUIRE(Sign(0.0f) == 0.0f);
    REQUIRE(Sign(5.0f) == 1.0f);
    REQUIRE(Floor(1.9f) == 1.0f);
    REQUIRE(Ceil(1.1f) == 2.0f);
    REQUIRE(Round(1.6f) == 2.0f);
    REQUIRE_THAT(Fract(2.25f), WithinAbs(0.25f, 0.00001f));
    REQUIRE_THAT(Mod(5.5f, 2.0f), WithinAbs(1.5f, 0.00001f));
    REQUIRE_THAT(Pow(2.0f, 3.0f), WithinAbs(8.0f, 0.00001f));
  }
}
