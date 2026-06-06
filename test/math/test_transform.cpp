#include "gecko/math/transform.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace gecko;
using namespace gecko::math;
using Catch::Matchers::WithinAbs;

TEST_CASE("Transform point and vector", "[transform]")
{
  Transform transform {{5.0f, 0.0f, 0.0f}, Quat::AxisAngle({0.0f, 1.0f, 0.0f}, ToRadians(90.0f)), {2.0f, 2.0f, 2.0f}};

  Float3 point = TransformPoint(transform, {1.0f, 0.0f, 0.0f});
  REQUIRE_THAT(point.X, WithinAbs(5.0f, 0.00001f));
  REQUIRE_THAT(point.Y, WithinAbs(0.0f, 0.00001f));
  REQUIRE_THAT(point.Z, WithinAbs(-2.0f, 0.00001f));

  Float3 vector = TransformVector(transform, {1.0f, 0.0f, 0.0f});
  REQUIRE_THAT(vector.X, WithinAbs(0.0f, 0.00001f));
  REQUIRE_THAT(vector.Y, WithinAbs(0.0f, 0.00001f));
  REQUIRE_THAT(vector.Z, WithinAbs(-2.0f, 0.00001f));
}

TEST_CASE("Transform matrix matches direct transform", "[transform]")
{
  Transform transform {{1.0f, 2.0f, 3.0f}, Quat::AxisAngle({0.0f, 0.0f, 1.0f}, ToRadians(90.0f)), {2.0f, 3.0f, 4.0f}};
  Float3 point {1.0f, 0.0f, 0.0f};

  Float3 direct = TransformPoint(transform, point);
  Float3 matrix = TransformPoint(ToMatrix(transform), point);

  REQUIRE_THAT(matrix.X, WithinAbs(direct.X, 0.00001f));
  REQUIRE_THAT(matrix.Y, WithinAbs(direct.Y, 0.00001f));
  REQUIRE_THAT(matrix.Z, WithinAbs(direct.Z, 0.00001f));
}

TEST_CASE("Transform composition", "[transform]")
{
  Transform parent {{10.0f, 0.0f, 0.0f}, Quat::AxisAngle({0.0f, 1.0f, 0.0f}, ToRadians(90.0f)), {2.0f, 2.0f, 2.0f}};
  Transform child {{1.0f, 0.0f, 0.0f}};
  Transform combined = parent * child;

  Float3 origin = TransformPoint(combined, {0.0f, 0.0f, 0.0f});
  REQUIRE_THAT(origin.X, WithinAbs(10.0f, 0.00001f));
  REQUIRE_THAT(origin.Y, WithinAbs(0.0f, 0.00001f));
  REQUIRE_THAT(origin.Z, WithinAbs(-2.0f, 0.00001f));
}
