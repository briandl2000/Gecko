#include "gecko/math/vector.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace gecko;
using namespace gecko::math;
using Catch::Matchers::WithinAbs;

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

    Float2 componentResult = a * b;
    REQUIRE(componentResult.X == 3.0f);
    REQUIRE(componentResult.Y == 8.0f);
  }

  SECTION("Division")
  {
    Float2 result = a / 2.0f;
    REQUIRE(result.X == 1.5f);
    REQUIRE(result.Y == 2.0f);

    Float2 componentResult = a / b;
    REQUIRE(componentResult.X == 3.0f);
    REQUIRE(componentResult.Y == 2.0f);
  }

  SECTION("Compound assignment")
  {
    Float2 result = a;
    result += b;
    REQUIRE(result == Float2 {4.0f, 6.0f});
    result -= b;
    REQUIRE(result == a);
    result *= b;
    REQUIRE(result == Float2 {3.0f, 8.0f});
    result /= b;
    REQUIRE(result == a);
    result *= 2.0f;
    REQUIRE(result == Float2 {6.0f, 8.0f});
    result /= 2.0f;
    REQUIRE(result == a);
  }

  SECTION("Unary negation")
  {
    Float2 result = -a;
    REQUIRE(result == Float2 {-3.0f, -4.0f});
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

  SECTION("Distance")
  {
    REQUIRE_THAT(Distance(a, b), WithinAbs(2.828427f, 0.00001f));
    REQUIRE_THAT(DistanceSquared(a, b), WithinAbs(8.0f, 0.00001f));
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

  SECTION("Reflect")
  {
    Float2 result = Reflect({1.0f, -1.0f}, {0.0f, 1.0f});
    REQUIRE(result == Float2 {1.0f, 1.0f});
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

    Float3 componentResult = a * b;
    REQUIRE(componentResult == Float3 {4.0f, 10.0f, 18.0f});
  }

  SECTION("Division")
  {
    Float3 result = b / a;
    REQUIRE(result == Float3 {4.0f, 2.5f, 2.0f});
  }

  SECTION("Compound assignment")
  {
    Float3 result = a;
    result += b;
    REQUIRE(result == Float3 {5.0f, 7.0f, 9.0f});
    result -= b;
    REQUIRE(result == a);
    result *= b;
    REQUIRE(result == Float3 {4.0f, 10.0f, 18.0f});
    result /= b;
    REQUIRE(result == a);
    result *= 2.0f;
    REQUIRE(result == Float3 {2.0f, 4.0f, 6.0f});
    result /= 2.0f;
    REQUIRE(result == a);
  }

  SECTION("Unary negation")
  {
    REQUIRE(-a == Float3 {-1.0f, -2.0f, -3.0f});
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
    REQUIRE_THAT(Distance(v, {0.0f, 0.0f, 0.0f}), WithinAbs(5.0f, 0.00001f));
    REQUIRE_THAT(DistanceSquared(v, {0.0f, 0.0f, 0.0f}), WithinAbs(25.0f, 0.00001f));
  }

  SECTION("Normalized")
  {
    Float3 v {5.0f, 0.0f, 0.0f};
    Float3 result = Normalized(v);
    REQUIRE_THAT(result.X, WithinAbs(1.0f, 0.00001f));
    REQUIRE_THAT(Length(result), WithinAbs(1.0f, 0.00001f));
  }

  SECTION("Reflect")
  {
    Float3 result = Reflect({1.0f, -2.0f, 3.0f}, {0.0f, 1.0f, 0.0f});
    REQUIRE(result == Float3 {1.0f, 2.0f, 3.0f});
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

  SECTION("Component arithmetic")
  {
    REQUIRE(a * b == Float4 {5.0f, 12.0f, 21.0f, 32.0f});
    REQUIRE(b / a == Float4 {5.0f, 3.0f, 2.3333333f, 2.0f});
    REQUIRE(-a == Float4 {-1.0f, -2.0f, -3.0f, -4.0f});

    Float4 result = a;
    result += b;
    REQUIRE(result == Float4 {6.0f, 8.0f, 10.0f, 12.0f});
    result -= b;
    REQUIRE(result == a);
    result *= b;
    REQUIRE(result == Float4 {5.0f, 12.0f, 21.0f, 32.0f});
    result /= b;
    REQUIRE(result == a);
    result *= 2.0f;
    REQUIRE(result == Float4 {2.0f, 4.0f, 6.0f, 8.0f});
    result /= 2.0f;
    REQUIRE(result == a);
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
    REQUIRE_THAT(Distance(a, b), WithinAbs(8.0f, 0.00001f));
    REQUIRE_THAT(DistanceSquared(a, b), WithinAbs(64.0f, 0.00001f));
  }

  SECTION("Reflect")
  {
    Float4 result = Reflect({1.0f, -2.0f, 3.0f, 0.0f}, {0.0f, 1.0f, 0.0f, 0.0f});
    REQUIRE(result == Float4 {1.0f, 2.0f, 3.0f, 0.0f});
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

  SECTION("Component arithmetic")
  {
    REQUIRE(a * b == Int2 {3, 8});
    REQUIRE(a / b == Int2 {3, 2});
    REQUIRE(2 * b == Int2 {2, 4});
    REQUIRE(-a == Int2 {-3, -4});

    Int2 result = a;
    result += b;
    REQUIRE(result == Int2 {4, 6});
    result -= b;
    REQUIRE(result == a);
    result *= b;
    REQUIRE(result == Int2 {3, 8});
    result /= b;
    REQUIRE(result == a);
    result *= 2;
    REQUIRE(result == Int2 {6, 8});
    result /= 2;
    REQUIRE(result == a);
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

  SECTION("Abs")
  {
    REQUIRE(Abs(Int2 {-3, -4}) == Int2 {3, 4});
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

  SECTION("Component arithmetic")
  {
    REQUIRE(a * b == Int3 {4, 10, 18});
    REQUIRE(b / a == Int3 {4, 2, 2});
    REQUIRE(2 * a == Int3 {2, 4, 6});
    REQUIRE(-a == Int3 {-1, -2, -3});

    Int3 result = a;
    result += b;
    REQUIRE(result == Int3 {5, 7, 9});
    result -= b;
    REQUIRE(result == a);
    result *= b;
    REQUIRE(result == Int3 {4, 10, 18});
    result /= b;
    REQUIRE(result == a);
    result *= 2;
    REQUIRE(result == Int3 {2, 4, 6});
    result /= 2;
    REQUIRE(result == a);
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

  SECTION("Abs")
  {
    REQUIRE(Abs(Int3 {-1, -2, -3}) == Int3 {1, 2, 3});
  }
}

TEST_CASE("Int4 operations", "[vector][Int4]")
{
  Int4 a {1, 2, 3, 4};
  Int4 b {5, 6, 7, 8};

  SECTION("Basic arithmetic")
  {
    REQUIRE(a + b == Int4 {6, 8, 10, 12});
    REQUIRE(b - a == Int4 {4, 4, 4, 4});
    REQUIRE(a * b == Int4 {5, 12, 21, 32});
    REQUIRE(b / a == Int4 {5, 3, 2, 2});
    REQUIRE(2 * a == Int4 {2, 4, 6, 8});
    REQUIRE(-a == Int4 {-1, -2, -3, -4});
  }

  SECTION("Compound assignment")
  {
    Int4 result = a;
    result += b;
    REQUIRE(result == Int4 {6, 8, 10, 12});
    result -= b;
    REQUIRE(result == a);
    result *= b;
    REQUIRE(result == Int4 {5, 12, 21, 32});
    result /= b;
    REQUIRE(result == a);
    result *= 2;
    REQUIRE(result == Int4 {2, 4, 6, 8});
    result /= 2;
    REQUIRE(result == a);
  }

  SECTION("Vector helpers")
  {
    REQUIRE(Dot(a, b) == 70);
    REQUIRE(Min(a, b) == a);
    REQUIRE(Max(a, b) == b);
    REQUIRE(Clamp(Int4 {-1, 2, 30, 4}, {0, 0, 0, 0}, {10, 10, 10, 10}) == Int4 {0, 2, 10, 4});
    REQUIRE(Abs(Int4 {-1, -2, -3, -4}) == a);
  }
}
