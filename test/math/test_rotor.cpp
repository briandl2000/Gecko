#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "gecko/math/rotor.h"

using namespace gecko;
using namespace gecko::math;
using Catch::Matchers::WithinAbs;

TEST_CASE("Rotor identity", "[rotor]")
{
  Rotor r;  // Default constructor creates identity
  
  REQUIRE(r.S == 1.0f);
  REQUIRE(r.E23 == 0.0f);
  REQUIRE(r.E31 == 0.0f);
  REQUIRE(r.E12 == 0.0f);
}

TEST_CASE("Rotor from axis-angle", "[rotor][construction]")
{
  SECTION("90 degree rotation around Z-axis")
  {
    Rotor r = Rotor::AxisAngle({0.0f, 0.0f, 1.0f}, ToRadians(90.0f));
    Float3 v {1.0f, 0.0f, 0.0f};
    Float3 result = Rotate(r, v);
    
    REQUIRE_THAT(result.X, WithinAbs(0.0f, 0.00001f));
    REQUIRE_THAT(result.Y, WithinAbs(1.0f, 0.00001f));
    REQUIRE_THAT(result.Z, WithinAbs(0.0f, 0.00001f));
  }
  
  SECTION("180 degree rotation around Z-axis")
  {
    Rotor r = Rotor::AxisAngle({0.0f, 0.0f, 1.0f}, ToRadians(180.0f));
    Float3 v {1.0f, 0.0f, 0.0f};
    Float3 result = Rotate(r, v);
    
    REQUIRE_THAT(result.X, WithinAbs(-1.0f, 0.00001f));
    REQUIRE_THAT(result.Y, WithinAbs(0.0f, 0.00001f));
    REQUIRE_THAT(result.Z, WithinAbs(0.0f, 0.00001f));
  }
  
  SECTION("90 degree rotation around X-axis")
  {
    Rotor r = Rotor::AxisAngle({1.0f, 0.0f, 0.0f}, ToRadians(90.0f));
    Float3 v {0.0f, 1.0f, 0.0f};
    Float3 result = Rotate(r, v);
    
    REQUIRE_THAT(result.X, WithinAbs(0.0f, 0.00001f));
    REQUIRE_THAT(result.Y, WithinAbs(0.0f, 0.00001f));
    REQUIRE_THAT(result.Z, WithinAbs(1.0f, 0.00001f));
  }
  
  SECTION("90 degree rotation around Y-axis")
  {
    Rotor r = Rotor::AxisAngle({0.0f, 1.0f, 0.0f}, ToRadians(90.0f));
    Float3 v {1.0f, 0.0f, 0.0f};
    Float3 result = Rotate(r, v);
    
    REQUIRE_THAT(result.X, WithinAbs(0.0f, 0.00001f));
    REQUIRE_THAT(result.Y, WithinAbs(0.0f, 0.00001f));
    REQUIRE_THAT(result.Z, WithinAbs(-1.0f, 0.00001f));
  }
}

TEST_CASE("Rotor FromTo", "[rotor][construction]")
{
  SECTION("Rotate X-axis to Y-axis")
  {
    Float3 from {1.0f, 0.0f, 0.0f};
    Float3 to {0.0f, 1.0f, 0.0f};
    Rotor r = Rotor::FromTo(from, to);
    
    Float3 result = Rotate(r, from);
    REQUIRE_THAT(result.X, WithinAbs(0.0f, 0.00001f));
    REQUIRE_THAT(result.Y, WithinAbs(1.0f, 0.00001f));
    REQUIRE_THAT(result.Z, WithinAbs(0.0f, 0.00001f));
  }
  
  SECTION("Rotate X-axis to negative X-axis")
  {
    Float3 from {1.0f, 0.0f, 0.0f};
    Float3 to {-1.0f, 0.0f, 0.0f};
    Rotor r = Rotor::FromTo(from, to);
    
    Float3 result = Rotate(r, from);
    REQUIRE_THAT(result.X, WithinAbs(-1.0f, 0.00001f));
    REQUIRE_THAT(result.Y, WithinAbs(0.0f, 0.00001f));
    REQUIRE_THAT(result.Z, WithinAbs(0.0f, 0.00001f));
  }
  
  SECTION("Same vector returns identity")
  {
    Float3 v {1.0f, 0.0f, 0.0f};
    Rotor r = Rotor::FromTo(v, v);
    
    Float3 result = Rotate(r, v);
    REQUIRE_THAT(result.X, WithinAbs(1.0f, 0.00001f));
    REQUIRE_THAT(result.Y, WithinAbs(0.0f, 0.00001f));
    REQUIRE_THAT(result.Z, WithinAbs(0.0f, 0.00001f));
  }
}

TEST_CASE("Rotor composition", "[rotor][operations]")
{
  SECTION("Two 90-degree rotations make 180")
  {
    Rotor r1 = Rotor::AxisAngle({0.0f, 0.0f, 1.0f}, ToRadians(90.0f));
    Rotor r2 = Rotor::AxisAngle({0.0f, 0.0f, 1.0f}, ToRadians(90.0f));
    Rotor combined = r2 * r1;
    
    Float3 v {1.0f, 0.0f, 0.0f};
    Float3 result = Rotate(combined, v);
    
    REQUIRE_THAT(result.X, WithinAbs(-1.0f, 0.00001f));
    REQUIRE_THAT(result.Y, WithinAbs(0.0f, 0.00001f));
    REQUIRE_THAT(result.Z, WithinAbs(0.0f, 0.00001f));
  }
  
  SECTION("Combining rotations around different axes")
  {
    Rotor r1 = Rotor::AxisAngle({0.0f, 0.0f, 1.0f}, ToRadians(90.0f));
    Rotor r2 = Rotor::AxisAngle({1.0f, 0.0f, 0.0f}, ToRadians(45.0f));
    Rotor combined = r2 * r1;
    
    Float3 v {1.0f, 0.0f, 0.0f};
    Float3 result = Rotate(combined, v);
    
    // After 90° Z-rotation: (1,0,0) -> (0,1,0)
    // After 45° X-rotation: (0,1,0) -> (0, cos(45), sin(45))
    REQUIRE_THAT(result.X, WithinAbs(0.0f, 0.00001f));
    REQUIRE_THAT(result.Y, WithinAbs(0.707107f, 0.00001f));
    REQUIRE_THAT(result.Z, WithinAbs(0.707107f, 0.00001f));
  }
}

TEST_CASE("Rotor reverse", "[rotor][operations]")
{
  Rotor r = Rotor::AxisAngle({0.0f, 0.0f, 1.0f}, ToRadians(90.0f));
  Rotor rev = Reverse(r);
  
  Float3 v {1.0f, 0.0f, 0.0f};
  Float3 rotated = Rotate(r, v);
  Float3 unrotated = Rotate(rev, rotated);
  
  // Should return to original
  REQUIRE_THAT(unrotated.X, WithinAbs(1.0f, 0.00001f));
  REQUIRE_THAT(unrotated.Y, WithinAbs(0.0f, 0.00001f));
  REQUIRE_THAT(unrotated.Z, WithinAbs(0.0f, 0.00001f));
}

TEST_CASE("Rotor Slerp", "[rotor][interpolation]")
{
  Rotor r1 = Rotor::AxisAngle({0.0f, 1.0f, 0.0f}, 0.0f);
  Rotor r2 = Rotor::AxisAngle({0.0f, 1.0f, 0.0f}, ToRadians(180.0f));
  
  SECTION("t=0 returns first rotor")
  {
    Rotor result = Slerp(r1, r2, 0.0f);
    Float3 v {1.0f, 0.0f, 0.0f};
    Float3 rotated = Rotate(result, v);
    
    REQUIRE_THAT(rotated.X, WithinAbs(1.0f, 0.00001f));
    REQUIRE_THAT(rotated.Z, WithinAbs(0.0f, 0.00001f));
  }
  
  SECTION("t=1 returns second rotor")
  {
    Rotor result = Slerp(r1, r2, 1.0f);
    Float3 v {1.0f, 0.0f, 0.0f};
    Float3 rotated = Rotate(result, v);
    
    REQUIRE_THAT(rotated.X, WithinAbs(-1.0f, 0.00001f));
    REQUIRE_THAT(rotated.Z, WithinAbs(0.0f, 0.00001f));
  }
  
  SECTION("t=0.5 is halfway rotation (90 degrees)")
  {
    Rotor result = Slerp(r1, r2, 0.5f);
    Float3 v {1.0f, 0.0f, 0.0f};
    Float3 rotated = Rotate(result, v);
    
    // Halfway between 0° and 180° should be 90°
    REQUIRE_THAT(rotated.X, WithinAbs(0.0f, 0.00001f));
    REQUIRE_THAT(rotated.Y, WithinAbs(0.0f, 0.00001f));
    // May be positive or negative 1 depending on interpolation direction
    REQUIRE_THAT(::gecko::math::Abs(rotated.Z), WithinAbs(1.0f, 0.00001f));
  }
}

TEST_CASE("Rotor Nlerp", "[rotor][interpolation]")
{
  Rotor r1 = Rotor::AxisAngle({0.0f, 0.0f, 1.0f}, 0.0f);
  Rotor r2 = Rotor::AxisAngle({0.0f, 0.0f, 1.0f}, ToRadians(90.0f));
  
  Rotor result = Nlerp(r1, r2, 0.5f);
  Float3 v {1.0f, 0.0f, 0.0f};
  Float3 rotated = Rotate(result, v);
  
  // Nlerp is approximate but should be close to halfway
  REQUIRE_THAT(Length(rotated), WithinAbs(1.0f, 0.00001f));
  REQUIRE(rotated.X > 0.0f);  // Between 0 and 1
  REQUIRE(rotated.Y > 0.0f);  // Between 0 and 1
}

TEST_CASE("Rotor preserves vector length", "[rotor][properties]")
{
  Rotor r = Rotor::AxisAngle({1.0f, 1.0f, 1.0f}, ToRadians(45.0f));
  Float3 v {3.0f, 4.0f, 5.0f};
  
  f32 originalLength = Length(v);
  Float3 rotated = Rotate(r, v);
  f32 rotatedLength = Length(rotated);
  
  REQUIRE_THAT(rotatedLength, WithinAbs(originalLength, 0.00001f));
}

TEST_CASE("Rotor identity behavior", "[rotor][properties]")
{
  Rotor identity;
  Float3 v {1.0f, 2.0f, 3.0f};
  Float3 result = Rotate(identity, v);
  
  REQUIRE(result.X == v.X);
  REQUIRE(result.Y == v.Y);
  REQUIRE(result.Z == v.Z);
}

TEST_CASE("Rotor ToPlanAngle", "[rotor][conversion]")
{
  SECTION("90 degree rotation")
  {
    Rotor r = Rotor::AxisAngle({0.0f, 0.0f, 1.0f}, ToRadians(90.0f));
    Float3 axis;
    f32 angle;
    ToPlanAngle(r, axis, angle);
    
    REQUIRE_THAT(angle, WithinAbs(ToRadians(90.0f), 0.00001f));
    // Axis should be normalized
    REQUIRE_THAT(Length(axis), WithinAbs(1.0f, 0.00001f));
  }
  
  SECTION("Identity rotation")
  {
    Rotor r;
    Float3 axis;
    f32 angle;
    ToPlanAngle(r, axis, angle);
    
    REQUIRE_THAT(angle, WithinAbs(0.0f, 0.00001f));
  }
}
