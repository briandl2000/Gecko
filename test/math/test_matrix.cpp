#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "gecko/math/matrix.h"

using namespace gecko;
using namespace gecko::math;
using Catch::Matchers::WithinAbs;

TEST_CASE("Float2x2 identity matrix", "[matrix][Float2x2]")
{
  Float2x2 identity = Float2x2::Identity();
  
  REQUIRE(identity.M00 == 1.0f);
  REQUIRE(identity.M01 == 0.0f);
  REQUIRE(identity.M10 == 0.0f);
  REQUIRE(identity.M11 == 1.0f);
}

TEST_CASE("Float2x2 rotation", "[matrix][Float2x2][rotation]")
{
  SECTION("90 degree rotation")
  {
    Float2x2 rot = Float2x2::Rotation(ToRadians(90.0f));
    Float2 v {1.0f, 0.0f};
    Float2 result = rot * v;
    
    REQUIRE_THAT(result.X, WithinAbs(0.0f, 0.00001f));
    REQUIRE_THAT(result.Y, WithinAbs(1.0f, 0.00001f));
  }
  
  SECTION("180 degree rotation")
  {
    Float2x2 rot = Float2x2::Rotation(ToRadians(180.0f));
    Float2 v {1.0f, 0.0f};
    Float2 result = rot * v;
    
    REQUIRE_THAT(result.X, WithinAbs(-1.0f, 0.00001f));
    REQUIRE_THAT(result.Y, WithinAbs(0.0f, 0.00001f));
  }
}

TEST_CASE("Float2x2 array access", "[matrix][Float2x2]")
{
  Float2x2 mat {1.0f, 2.0f, 3.0f, 4.0f};
  
  SECTION("Row access")
  {
    REQUIRE(mat[0].X == 1.0f);
    REQUIRE(mat[0].Y == 2.0f);
    REQUIRE(mat[1].X == 3.0f);
    REQUIRE(mat[1].Y == 4.0f);
  }
  
  SECTION("Data array access")
  {
    REQUIRE(mat.Data[0] == 1.0f);
    REQUIRE(mat.Data[1] == 2.0f);
    REQUIRE(mat.Data[2] == 3.0f);
    REQUIRE(mat.Data[3] == 4.0f);
  }
}

TEST_CASE("Float3x3 identity matrix", "[matrix][Float3x3]")
{
  Float3x3 identity = Float3x3::Identity();
  
  REQUIRE(identity.M00 == 1.0f);
  REQUIRE(identity.M11 == 1.0f);
  REQUIRE(identity.M22 == 1.0f);
  REQUIRE(identity.M01 == 0.0f);
  REQUIRE(identity.M10 == 0.0f);
}

TEST_CASE("Float3x3 rotation matrices", "[matrix][Float3x3][rotation]")
{
  SECTION("RotationX - 90 degrees")
  {
    Float3x3 rot = Float3x3::RotationX(ToRadians(90.0f));
    Float3 v {0.0f, 1.0f, 0.0f};
    Float3 result = rot * v;
    
    REQUIRE_THAT(result.X, WithinAbs(0.0f, 0.00001f));
    REQUIRE_THAT(result.Y, WithinAbs(0.0f, 0.00001f));
    REQUIRE_THAT(result.Z, WithinAbs(1.0f, 0.00001f));
  }
  
  SECTION("RotationY - 90 degrees")
  {
    Float3x3 rot = Float3x3::RotationY(ToRadians(90.0f));
    Float3 v {1.0f, 0.0f, 0.0f};
    Float3 result = rot * v;
    
    REQUIRE_THAT(result.X, WithinAbs(0.0f, 0.00001f));
    REQUIRE_THAT(result.Y, WithinAbs(0.0f, 0.00001f));
    REQUIRE_THAT(result.Z, WithinAbs(-1.0f, 0.00001f));
  }
  
  SECTION("RotationZ - 90 degrees")
  {
    Float3x3 rot = Float3x3::RotationZ(ToRadians(90.0f));
    Float3 v {1.0f, 0.0f, 0.0f};
    Float3 result = rot * v;
    
    REQUIRE_THAT(result.X, WithinAbs(0.0f, 0.00001f));
    REQUIRE_THAT(result.Y, WithinAbs(1.0f, 0.00001f));
    REQUIRE_THAT(result.Z, WithinAbs(0.0f, 0.00001f));
  }
}

TEST_CASE("Float4x4 identity matrix", "[matrix][Float4x4]")
{
  Float4x4 identity = Float4x4::Identity();
  
  REQUIRE(identity.M00 == 1.0f);
  REQUIRE(identity.M11 == 1.0f);
  REQUIRE(identity.M22 == 1.0f);
  REQUIRE(identity.M33 == 1.0f);
  
  for (int i = 0; i < 16; ++i) {
    if (i % 5 == 0) {  // Diagonal elements (0, 5, 10, 15)
      REQUIRE(identity.Data[i] == 1.0f);
    } else {
      REQUIRE(identity.Data[i] == 0.0f);
    }
  }
}

TEST_CASE("Float4x4 translation", "[matrix][Float4x4][transform]")
{
  Float4x4 trans = Float4x4::Translation({5.0f, 10.0f, 15.0f});
  Float3 point {1.0f, 2.0f, 3.0f};
  Float3 result = TransformPoint(trans, point);
  
  REQUIRE(result.X == 6.0f);
  REQUIRE(result.Y == 12.0f);
  REQUIRE(result.Z == 18.0f);
}

TEST_CASE("Float4x4 scale", "[matrix][Float4x4][transform]")
{
  Float4x4 scale = Float4x4::Scale({2.0f, 3.0f, 4.0f});
  Float3 point {1.0f, 2.0f, 3.0f};
  Float3 result = TransformPoint(scale, point);
  
  REQUIRE(result.X == 2.0f);
  REQUIRE(result.Y == 6.0f);
  REQUIRE(result.Z == 12.0f);
}

TEST_CASE("Float4x4 rotation matrices", "[matrix][Float4x4][rotation]")
{
  SECTION("RotationX - 90 degrees")
  {
    Float4x4 rot = Float4x4::RotationX(ToRadians(90.0f));
    Float3 v {0.0f, 1.0f, 0.0f};
    Float3 result = TransformVector(rot, v);
    
    REQUIRE_THAT(result.X, WithinAbs(0.0f, 0.00001f));
    REQUIRE_THAT(result.Y, WithinAbs(0.0f, 0.00001f));
    REQUIRE_THAT(result.Z, WithinAbs(1.0f, 0.00001f));
  }
  
  SECTION("RotationY - 90 degrees")
  {
    Float4x4 rot = Float4x4::RotationY(ToRadians(90.0f));
    Float3 v {1.0f, 0.0f, 0.0f};
    Float3 result = TransformVector(rot, v);
    
    REQUIRE_THAT(result.X, WithinAbs(0.0f, 0.00001f));
    REQUIRE_THAT(result.Y, WithinAbs(0.0f, 0.00001f));
    REQUIRE_THAT(result.Z, WithinAbs(-1.0f, 0.00001f));
  }
  
  SECTION("RotationZ - 90 degrees")
  {
    Float4x4 rot = Float4x4::RotationZ(ToRadians(90.0f));
    Float3 v {1.0f, 0.0f, 0.0f};
    Float3 result = TransformVector(rot, v);
    
    REQUIRE_THAT(result.X, WithinAbs(0.0f, 0.00001f));
    REQUIRE_THAT(result.Y, WithinAbs(1.0f, 0.00001f));
    REQUIRE_THAT(result.Z, WithinAbs(0.0f, 0.00001f));
  }
}

TEST_CASE("Float4x4 LookAt camera", "[matrix][Float4x4][camera]")
{
  Float4x4 view = Float4x4::LookAt(
    {0.0f, 0.0f, 5.0f},  // eye
    {0.0f, 0.0f, 0.0f},  // target
    {0.0f, 1.0f, 0.0f}   // up
  );
  
  // LookAt should create a view matrix that transforms world to camera space
  // The eye position should be at origin in camera space
  Float3 eyeInCameraSpace = TransformPoint(view, {0.0f, 0.0f, 5.0f});
  REQUIRE_THAT(eyeInCameraSpace.X, WithinAbs(0.0f, 0.00001f));
  REQUIRE_THAT(eyeInCameraSpace.Y, WithinAbs(0.0f, 0.00001f));
  REQUIRE_THAT(eyeInCameraSpace.Z, WithinAbs(0.0f, 0.00001f));
}

TEST_CASE("Float4x4 Orthographic projection", "[matrix][Float4x4][camera]")
{
  Float4x4 ortho = Float4x4::Orthographic(
    -10.0f, 10.0f,  // left, right
    -10.0f, 10.0f,  // bottom, top
    0.1f, 100.0f    // near, far
  );
  
  // Check that the matrix is not identity and has correct structure
  REQUIRE(ortho.M00 != 0.0f);
  REQUIRE(ortho.M11 != 0.0f);
  REQUIRE(ortho.M22 != 0.0f);
  REQUIRE(ortho.M33 == 1.0f);
}

TEST_CASE("Float4x4 Perspective projection", "[matrix][Float4x4][camera]")
{
  Float4x4 persp = Float4x4::Perspective(
    ToRadians(60.0f),  // fovY
    16.0f / 9.0f,       // aspect
    0.1f, 100.0f        // near, far
  );
  
  // Check that the matrix has correct structure for perspective
  REQUIRE(persp.M00 != 0.0f);
  REQUIRE(persp.M11 != 0.0f);
  REQUIRE(persp.M22 != 0.0f);
  REQUIRE(persp.M32 == -1.0f);  // Perspective projection has -1 here
  REQUIRE(persp.M33 == 0.0f);   // And 0 at bottom-right
}

TEST_CASE("Float4x4 matrix multiplication", "[matrix][Float4x4]")
{
  Float4x4 trans = Float4x4::Translation({5.0f, 0.0f, 0.0f});
  Float4x4 scale = Float4x4::Scale({2.0f, 2.0f, 2.0f});
  
  // Combined: first scale, then translate
  Float4x4 combined = trans * scale;
  Float3 point {1.0f, 1.0f, 1.0f};
  Float3 result = TransformPoint(combined, point);
  
  // Should be scaled to (2,2,2) then translated by (5,0,0)
  REQUIRE(result.X == 7.0f);
  REQUIRE(result.Y == 2.0f);
  REQUIRE(result.Z == 2.0f);
}

TEST_CASE("Matrix row access", "[matrix][Float4x4]")
{
  Float4x4 mat = Float4x4::Identity();
  
  SECTION("Row 0")
  {
    Float4 row0 = mat[0];
    REQUIRE(row0.X == 1.0f);
    REQUIRE(row0.Y == 0.0f);
    REQUIRE(row0.Z == 0.0f);
    REQUIRE(row0.W == 0.0f);
  }
  
  SECTION("Row modification")
  {
    mat[0] = Float4{2.0f, 3.0f, 4.0f, 5.0f};
    REQUIRE(mat.M00 == 2.0f);
    REQUIRE(mat.M01 == 3.0f);
    REQUIRE(mat.M02 == 4.0f);
    REQUIRE(mat.M03 == 5.0f);
  }
}
