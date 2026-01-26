#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "gecko/math/vector.h"

using namespace gecko;
using namespace gecko::math;
using Catch::Matchers::WithinAbs;

TEST_CASE("Math constants are correct", "[vector][constants]")
{
  REQUIRE_THAT(Pi, WithinAbs(3.14159265f, 0.00001f));
  REQUIRE_THAT(TwoPi, WithinAbs(6.28318530f, 0.00001f));
  REQUIRE_THAT(HalfPi, WithinAbs(1.57079632f, 0.00001f));
}

TEST_CASE("Angle conversions", "[vector][utils]")
{
  REQUIRE_THAT(ToRadians(180.0f), WithinAbs(Pi, 0.00001f));
  REQUIRE_THAT(ToRadians(90.0f), WithinAbs(HalfPi, 0.00001f));
  REQUIRE_THAT(ToDegrees(Pi), WithinAbs(180.0f, 0.00001f));
  REQUIRE_THAT(ToDegrees(HalfPi), WithinAbs(90.0f, 0.00001f));
}

TEST_CASE("Scalar utility functions", "[vector][utils]")
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
}

TEST_CASE("Float2 basic operations", "[vector][Float2]")
{
  Float2 a {3.0f, 4.0f};
  Float2 b {1.0f, 2.0f};

  SECTION("Addition")
  {
    Float2 result = a + b;
    REQUIRE(result.X == 4.0f);
    REQUIRE(result.Y == 6.0f);
  }

  SECTION("Subtraction")
  {
    Float2 result = a - b;
    REQUIRE(result.X == 2.0f);
    REQUIRE(result.Y == 2.0f);
  }

  SECTION("Multiplication")
  {
    Float2 result = a * 2.0f;
    REQUIRE(result.X == 6.0f);
    REQUIRE(result.Y == 8.0f);
  }

  SECTION("Division")
  {
    Float2 result = a / 2.0f;
    REQUIRE(result.X == 1.5f);
    REQUIRE(result.Y == 2.0f);
  }

  SECTION("Array access")
  {
    REQUIRE(a[0] == 3.0f);
    REQUIRE(a[1] == 4.0f);
    REQUIRE(a.Data[0] == 3.0f);
    REQUIRE(a.Data[1] == 4.0f);
  }
}

TEST_CASE("Float2 vector operations", "[vector][Float2]")
{
  Float2 a {3.0f, 4.0f};
  Float2 b {1.0f, 2.0f};

  SECTION("Dot product")
  {
    f32 result = Dot(a, b);
    REQUIRE(result == 11.0f);  // 3*1 + 4*2
  }

  SECTION("Cross product (2D)")
  {
    f32 result = Cross(a, b);
    REQUIRE(result == 2.0f);  // 3*2 - 4*1
  }

  SECTION("Length")
  {
    REQUIRE_THAT(Length(a), WithinAbs(5.0f, 0.00001f));
  }

  SECTION("Length squared")
  {
    REQUIRE_THAT(LengthSquared(a), WithinAbs(25.0f, 0.00001f));
  }

  SECTION("Normalized")
  {
    Float2 result = Normalized(a);
    REQUIRE_THAT(result.X, WithinAbs(0.6f, 0.00001f));
    REQUIRE_THAT(result.Y, WithinAbs(0.8f, 0.00001f));
    REQUIRE_THAT(Length(result), WithinAbs(1.0f, 0.00001f));
  }

  SECTION("Min and Max")
  {
    Float2 minVec = Min(a, b);
    Float2 maxVec = Max(a, b);
    REQUIRE(minVec.X == 1.0f);
    REQUIRE(minVec.Y == 2.0f);
    REQUIRE(maxVec.X == 3.0f);
    REQUIRE(maxVec.Y == 4.0f);
  }

  SECTION("Clamp")
  {
    Float2 val {-1.0f, 10.0f};
    Float2 result = Clamp(val, {0.0f, 0.0f}, {5.0f, 5.0f});
    REQUIRE(result.X == 0.0f);
    REQUIRE(result.Y == 5.0f);
  }

  SECTION("Lerp")
  {
    Float2 result = Lerp(a, b, 0.5f);
    REQUIRE_THAT(result.X, WithinAbs(2.0f, 0.00001f));
    REQUIRE_THAT(result.Y, WithinAbs(3.0f, 0.00001f));
  }

  SECTION("Abs")
  {
    Float2 neg {-3.0f, -4.0f};
    Float2 result = Abs(neg);
    REQUIRE(result.X == 3.0f);
    REQUIRE(result.Y == 4.0f);
  }
}

TEST_CASE("Float3 basic operations", "[vector][Float3]")
{
  Float3 a {1.0f, 2.0f, 3.0f};
  Float3 b {4.0f, 5.0f, 6.0f};

  SECTION("Addition")
  {
    Float3 result = a + b;
    REQUIRE(result.X == 5.0f);
    REQUIRE(result.Y == 7.0f);
    REQUIRE(result.Z == 9.0f);
  }

  SECTION("Subtraction")
  {
    Float3 result = b - a;
    REQUIRE(result.X == 3.0f);
    REQUIRE(result.Y == 3.0f);
    REQUIRE(result.Z == 3.0f);
  }

  SECTION("Multiplication")
  {
    Float3 result = a * 2.0f;
    REQUIRE(result.X == 2.0f);
    REQUIRE(result.Y == 4.0f);
    REQUIRE(result.Z == 6.0f);
  }

  SECTION("Array access")
  {
    REQUIRE(a[0] == 1.0f);
    REQUIRE(a[1] == 2.0f);
    REQUIRE(a[2] == 3.0f);
  }
}

TEST_CASE("Float3 vector operations", "[vector][Float3]")
{
  Float3 a {1.0f, 0.0f, 0.0f};
  Float3 b {0.0f, 1.0f, 0.0f};

  SECTION("Dot product")
  {
    REQUIRE(Dot(a, b) == 0.0f);
    REQUIRE(Dot(a, a) == 1.0f);
  }

  SECTION("Cross product")
  {
    Float3 result = Cross(a, b);
    REQUIRE(result.X == 0.0f);
    REQUIRE(result.Y == 0.0f);
    REQUIRE(result.Z == 1.0f);
  }

  SECTION("Length")
  {
    Float3 v {3.0f, 4.0f, 0.0f};
    REQUIRE_THAT(Length(v), WithinAbs(5.0f, 0.00001f));
  }

  SECTION("Normalized")
  {
    Float3 v {5.0f, 0.0f, 0.0f};
    Float3 result = Normalized(v);
    REQUIRE_THAT(result.X, WithinAbs(1.0f, 0.00001f));
    REQUIRE_THAT(Length(result), WithinAbs(1.0f, 0.00001f));
  }
}

TEST_CASE("Float4 basic operations", "[vector][Float4]")
{
  Float4 a {1.0f, 2.0f, 3.0f, 4.0f};
  Float4 b {5.0f, 6.0f, 7.0f, 8.0f};

  SECTION("Addition")
  {
    Float4 result = a + b;
    REQUIRE(result.X == 6.0f);
    REQUIRE(result.Y == 8.0f);
    REQUIRE(result.Z == 10.0f);
    REQUIRE(result.W == 12.0f);
  }

  SECTION("Array access")
  {
    REQUIRE(a[0] == 1.0f);
    REQUIRE(a[1] == 2.0f);
    REQUIRE(a[2] == 3.0f);
    REQUIRE(a[3] == 4.0f);
  }

  SECTION("Dot product")
  {
    f32 result = Dot(a, b);
    REQUIRE(result == 70.0f);  // 1*5 + 2*6 + 3*7 + 4*8
  }
}

TEST_CASE("Int2 operations", "[vector][Int2]")
{
  Int2 a {3, 4};
  Int2 b {1, 2};

  SECTION("Addition")
  {
    Int2 result = a + b;
    REQUIRE(result.X == 4);
    REQUIRE(result.Y == 6);
  }

  SECTION("Dot product")
  {
    i32 result = Dot(a, b);
    REQUIRE(result == 11);  // 3*1 + 4*2
  }

  SECTION("Min and Max")
  {
    Int2 minVec = Min(a, b);
    Int2 maxVec = Max(a, b);
    REQUIRE(minVec.X == 1);
    REQUIRE(minVec.Y == 2);
    REQUIRE(maxVec.X == 3);
    REQUIRE(maxVec.Y == 4);
  }

  SECTION("Clamp")
  {
    Int2 val {-1, 10};
    Int2 result = Clamp(val, {0, 0}, {5, 5});
    REQUIRE(result.X == 0);
    REQUIRE(result.Y == 5);
  }
}

TEST_CASE("Int3 operations", "[vector][Int3]")
{
  Int3 a {1, 2, 3};
  Int3 b {4, 5, 6};

  SECTION("Addition")
  {
    Int3 result = a + b;
    REQUIRE(result.X == 5);
    REQUIRE(result.Y == 7);
    REQUIRE(result.Z == 9);
  }

  SECTION("Dot product")
  {
    i32 result = Dot(a, b);
    REQUIRE(result == 32);  // 1*4 + 2*5 + 3*6
  }

  SECTION("Min and Max")
  {
    Int3 minVec = Min(a, b);
    Int3 maxVec = Max(a, b);
    REQUIRE(minVec.X == 1);
    REQUIRE(minVec.Y == 2);
    REQUIRE(minVec.Z == 3);
    REQUIRE(maxVec.X == 4);
    REQUIRE(maxVec.Y == 5);
    REQUIRE(maxVec.Z == 6);
  }
}
