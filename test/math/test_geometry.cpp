#include "gecko/math/math.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace gecko;
using namespace gecko::math;
using Catch::Matchers::WithinAbs;

TEST_CASE("Aabb3 helpers", "[aabb][Aabb3]")
{
  Aabb3 box {{-1.0f, -2.0f, -3.0f}, {3.0f, 4.0f, 5.0f}};

  REQUIRE(Size(box) == Float3 {4.0f, 6.0f, 8.0f});
  REQUIRE(Extents(box) == Float3 {2.0f, 3.0f, 4.0f});
  REQUIRE(Center(box) == Float3 {1.0f, 1.0f, 1.0f});
  REQUIRE_THAT(Volume(box), WithinAbs(192.0f, 0.00001f));
  REQUIRE_THAT(SurfaceArea(box), WithinAbs(208.0f, 0.00001f));

  REQUIRE(Contains(box, {0.0f, 0.0f, 0.0f}));
  REQUIRE_FALSE(Contains(box, {10.0f, 0.0f, 0.0f}));
  REQUIRE(Contains(box, Aabb3 {{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}));
  REQUIRE(Intersects(box, Aabb3 {{2.0f, 3.0f, 4.0f}, {10.0f, 10.0f, 10.0f}}));
  REQUIRE_FALSE(Intersects(box, Aabb3 {{4.0f, 0.0f, 0.0f}, {5.0f, 1.0f, 1.0f}}));
  REQUIRE(Clamp(box, {10.0f, 0.0f, -10.0f}) == Float3 {3.0f, 0.0f, -3.0f});
  REQUIRE(Expand(box, {10.0f, 0.0f, 0.0f}).Max == Float3 {10.0f, 4.0f, 5.0f});
  REQUIRE(Union(box, Aabb3 {{-10.0f, 0.0f, 0.0f}, {0.0f, 10.0f, 0.0f}}).Min == Float3 {-10.0f, -2.0f, -3.0f});
}

TEST_CASE("Plane helpers", "[plane]")
{
  Plane plane = Plane::FromPointNormal({0.0f, 2.0f, 0.0f}, {0.0f, 2.0f, 0.0f});

  REQUIRE_THAT(Distance(plane, {0.0f, 5.0f, 0.0f}), WithinAbs(3.0f, 0.00001f));
  REQUIRE(ProjectPoint(plane, {1.0f, 5.0f, 3.0f}) == Float3 {1.0f, 2.0f, 3.0f});

  Plane trianglePlane = Plane::FromTriangle({0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f});
  REQUIRE_THAT(Abs(Distance(trianglePlane, {0.0f, 5.0f, 0.0f})), WithinAbs(5.0f, 0.00001f));
}

TEST_CASE("Ray helpers", "[ray]")
{
  Ray ray {{0.0f, 0.0f, 5.0f}, {0.0f, 0.0f, -1.0f}};
  REQUIRE(PointAt(ray, 2.0f) == Float3 {0.0f, 0.0f, 3.0f});

  f32 t = 0.0f;
  REQUIRE(IntersectRayPlane(ray, Plane::FromPointNormal({0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 1.0f}), t));
  REQUIRE_THAT(t, WithinAbs(5.0f, 0.00001f));

  f32 tMin = 0.0f;
  f32 tMax = 0.0f;
  Aabb3 box {{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}};
  REQUIRE(IntersectRayAabb(ray, box, tMin, tMax));
  REQUIRE_THAT(tMin, WithinAbs(4.0f, 0.00001f));
  REQUIRE_THAT(tMax, WithinAbs(6.0f, 0.00001f));

  Ray miss {{5.0f, 5.0f, 5.0f}, {1.0f, 0.0f, 0.0f}};
  REQUIRE_FALSE(IntersectRayAabb(miss, box, tMin, tMax));
}
