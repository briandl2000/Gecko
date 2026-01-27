#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "gecko/math/quat.h"

using namespace gecko;
using namespace gecko::math;
using Catch::Matchers::WithinAbs;

TEST_CASE("Quat identity", "[quat]")
{
  Quat q;  // Default constructor creates identity
  
  REQUIRE(q.W == 1.0f);
  REQUIRE(q.X == 0.0f);
  REQUIRE(q.Y == 0.0f);
  REQUIRE(q.Z == 0.0f);
}

TEST_CASE("Quat from axis-angle", "[quat][construction]")
{
  SECTION("90 degree rotation around Z-axis")
  {
    Quat q = Quat::AxisAngle({0.0f, 0.0f, 1.0f}, ToRadians(90.0f));
    Float3 v {1.0f, 0.0f, 0.0f};
    Float3 result = Rotate(q, v);
    
    REQUIRE_THAT(result.X, WithinAbs(0.0f, 0.00001f));
    REQUIRE_THAT(result.Y, WithinAbs(1.0f, 0.00001f));
    REQUIRE_THAT(result.Z, WithinAbs(0.0f, 0.00001f));
  }
  
  SECTION("180 degree rotation around Z-axis")
  {
    Quat q = Quat::AxisAngle({0.0f, 0.0f, 1.0f}, ToRadians(180.0f));
    Float3 v {1.0f, 0.0f, 0.0f};
    Float3 result = Rotate(q, v);
    
    REQUIRE_THAT(result.X, WithinAbs(-1.0f, 0.00001f));
    REQUIRE_THAT(result.Y, WithinAbs(0.0f, 0.00001f));
    REQUIRE_THAT(result.Z, WithinAbs(0.0f, 0.00001f));
  }
  
  SECTION("90 degree rotation around X-axis")
  {
    Quat q = Quat::AxisAngle({1.0f, 0.0f, 0.0f}, ToRadians(90.0f));
    Float3 v {0.0f, 1.0f, 0.0f};
    Float3 result = Rotate(q, v);
    
    REQUIRE_THAT(result.X, WithinAbs(0.0f, 0.00001f));
    REQUIRE_THAT(result.Y, WithinAbs(0.0f, 0.00001f));
    REQUIRE_THAT(result.Z, WithinAbs(1.0f, 0.00001f));
  }
  
  SECTION("90 degree rotation around Y-axis")
  {
    Quat q = Quat::AxisAngle({0.0f, 1.0f, 0.0f}, ToRadians(90.0f));
    Float3 v {1.0f, 0.0f, 0.0f};
    Float3 result = Rotate(q, v);
    
    REQUIRE_THAT(result.X, WithinAbs(0.0f, 0.00001f));
    REQUIRE_THAT(result.Y, WithinAbs(0.0f, 0.00001f));
    REQUIRE_THAT(result.Z, WithinAbs(-1.0f, 0.00001f));
  }
}

TEST_CASE("Quat FromTo", "[quat][construction]")
{
  SECTION("Rotate X-axis to Y-axis")
  {
    Float3 from {1.0f, 0.0f, 0.0f};
    Float3 to {0.0f, 1.0f, 0.0f};
    Quat q = Quat::FromTo(from, to);
    
    Float3 result = Rotate(q, from);
    REQUIRE_THAT(result.X, WithinAbs(0.0f, 0.00001f));
    REQUIRE_THAT(result.Y, WithinAbs(1.0f, 0.00001f));
    REQUIRE_THAT(result.Z, WithinAbs(0.0f, 0.00001f));
  }
  
  SECTION("Rotate X-axis to negative X-axis")
  {
    Float3 from {1.0f, 0.0f, 0.0f};
    Float3 to {-1.0f, 0.0f, 0.0f};
    Quat q = Quat::FromTo(from, to);
    
    Float3 result = Rotate(q, from);
    REQUIRE_THAT(result.X, WithinAbs(-1.0f, 0.00001f));
    REQUIRE_THAT(result.Y, WithinAbs(0.0f, 0.00001f));
    REQUIRE_THAT(result.Z, WithinAbs(0.0f, 0.00001f));
  }
  
  SECTION("Same vector returns identity")
  {
    Float3 v {1.0f, 0.0f, 0.0f};
    Quat q = Quat::FromTo(v, v);
    
    Float3 result = Rotate(q, v);
    REQUIRE_THAT(result.X, WithinAbs(1.0f, 0.00001f));
    REQUIRE_THAT(result.Y, WithinAbs(0.0f, 0.00001f));
    REQUIRE_THAT(result.Z, WithinAbs(0.0f, 0.00001f));
  }
}

TEST_CASE("Quat composition", "[quat][operations]")
{
  SECTION("Two 90-degree rotations make 180")
  {
    Quat q1 = Quat::AxisAngle({0.0f, 0.0f, 1.0f}, ToRadians(90.0f));
    Quat q2 = Quat::AxisAngle({0.0f, 0.0f, 1.0f}, ToRadians(90.0f));
    Quat combined = q2 * q1;
    
    Float3 v {1.0f, 0.0f, 0.0f};
    Float3 result = Rotate(combined, v);
    
    REQUIRE_THAT(result.X, WithinAbs(-1.0f, 0.00001f));
    REQUIRE_THAT(result.Y, WithinAbs(0.0f, 0.00001f));
    REQUIRE_THAT(result.Z, WithinAbs(0.0f, 0.00001f));
  }
  
  SECTION("Combining rotations around different axes")
  {
    Quat q1 = Quat::AxisAngle({0.0f, 0.0f, 1.0f}, ToRadians(90.0f));
    Quat q2 = Quat::AxisAngle({1.0f, 0.0f, 0.0f}, ToRadians(45.0f));
    Quat combined = q2 * q1;
    
    Float3 v {1.0f, 0.0f, 0.0f};
    Float3 result = Rotate(combined, v);
    
    // After 90° Z-rotation: (1,0,0) -> (0,1,0)
    // After 45° X-rotation: (0,1,0) -> (0, cos(45), sin(45))
    REQUIRE_THAT(result.X, WithinAbs(0.0f, 0.00001f));
    REQUIRE_THAT(result.Y, WithinAbs(0.707107f, 0.00001f));
    REQUIRE_THAT(result.Z, WithinAbs(0.707107f, 0.00001f));
  }
}

TEST_CASE("Quat conjugate", "[quat][operations]")
{
  Quat q = Quat::AxisAngle({0.0f, 0.0f, 1.0f}, ToRadians(90.0f));
  Quat conj = Conjugate(q);
  
  Float3 v {1.0f, 0.0f, 0.0f};
  Float3 rotated = Rotate(q, v);
  Float3 unrotated = Rotate(conj, rotated);
  
  // Should return to original
  REQUIRE_THAT(unrotated.X, WithinAbs(1.0f, 0.00001f));
  REQUIRE_THAT(unrotated.Y, WithinAbs(0.0f, 0.00001f));
  REQUIRE_THAT(unrotated.Z, WithinAbs(0.0f, 0.00001f));
}

TEST_CASE("Quat Slerp", "[quat][interpolation]")
{
  Quat q1 = Quat::AxisAngle({0.0f, 1.0f, 0.0f}, 0.0f);
  Quat q2 = Quat::AxisAngle({0.0f, 1.0f, 0.0f}, ToRadians(180.0f));
  
  SECTION("t=0 returns first quaternion")
  {
    Quat result = Slerp(q1, q2, 0.0f);
    Float3 v {1.0f, 0.0f, 0.0f};
    Float3 rotated = Rotate(result, v);
    
    REQUIRE_THAT(rotated.X, WithinAbs(1.0f, 0.00001f));
    REQUIRE_THAT(rotated.Z, WithinAbs(0.0f, 0.00001f));
  }
  
  SECTION("t=1 returns second quaternion")
  {
    Quat result = Slerp(q1, q2, 1.0f);
    Float3 v {1.0f, 0.0f, 0.0f};
    Float3 rotated = Rotate(result, v);
    
    REQUIRE_THAT(rotated.X, WithinAbs(-1.0f, 0.00001f));
    REQUIRE_THAT(rotated.Z, WithinAbs(0.0f, 0.00001f));
  }
  
  SECTION("t=0.5 is halfway rotation (90 degrees)")
  {
    Quat result = Slerp(q1, q2, 0.5f);
    Float3 v {1.0f, 0.0f, 0.0f};
    Float3 rotated = Rotate(result, v);
    
    // Halfway between 0° and 180° should be 90°
    REQUIRE_THAT(rotated.X, WithinAbs(0.0f, 0.00001f));
    REQUIRE_THAT(rotated.Y, WithinAbs(0.0f, 0.00001f));
    // May be positive or negative 1 depending on interpolation direction
    REQUIRE_THAT(::gecko::math::Abs(rotated.Z), WithinAbs(1.0f, 0.00001f));
  }
}

TEST_CASE("Quat Nlerp", "[quat][interpolation]")
{
  Quat q1 = Quat::AxisAngle({0.0f, 0.0f, 1.0f}, 0.0f);
  Quat q2 = Quat::AxisAngle({0.0f, 0.0f, 1.0f}, ToRadians(90.0f));
  
  Quat result = Nlerp(q1, q2, 0.5f);
  Float3 v {1.0f, 0.0f, 0.0f};
  Float3 rotated = Rotate(result, v);
  
  // Nlerp is approximate but should be close to halfway
  REQUIRE_THAT(Length(rotated), WithinAbs(1.0f, 0.00001f));
  REQUIRE(rotated.X > 0.0f);  // Between 0 and 1
  REQUIRE(rotated.Y > 0.0f);  // Between 0 and 1
}

TEST_CASE("Quat preserves vector length", "[quat][properties]")
{
  Quat q = Quat::AxisAngle({1.0f, 1.0f, 1.0f}, ToRadians(45.0f));
  Float3 v {3.0f, 4.0f, 5.0f};
  
  f32 originalLength = Length(v);
  Float3 rotated = Rotate(q, v);
  f32 rotatedLength = Length(rotated);
  
  REQUIRE_THAT(rotatedLength, WithinAbs(originalLength, 0.00001f));
}

TEST_CASE("Quat identity behavior", "[quat][properties]")
{
  Quat identity;
  Float3 v {1.0f, 2.0f, 3.0f};
  Float3 result = Rotate(identity, v);
  
  REQUIRE(result.X == v.X);
  REQUIRE(result.Y == v.Y);
  REQUIRE(result.Z == v.Z);
}

TEST_CASE("Quat ToAxisAngle", "[quat][conversion]")
{
  SECTION("90 degree rotation")
  {
    Quat q = Quat::AxisAngle({0.0f, 0.0f, 1.0f}, ToRadians(90.0f));
    Float3 axis;
    f32 angle;
    ToAxisAngle(q, axis, angle);
    
    REQUIRE_THAT(angle, WithinAbs(ToRadians(90.0f), 0.00001f));
    // Axis should be normalized
    REQUIRE_THAT(Length(axis), WithinAbs(1.0f, 0.00001f));
  }
  
  SECTION("Identity rotation")
  {
    Quat q;
    Float3 axis;
    f32 angle;
    ToAxisAngle(q, axis, angle);
    
    REQUIRE_THAT(angle, WithinAbs(0.0f, 0.00001f));
  }
}
