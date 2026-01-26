#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "gecko/math/aabb.h"

using namespace gecko;
using namespace gecko::math;
using Catch::Matchers::WithinAbs;

TEST_CASE("Aabb2 construction", "[aabb][Aabb2]")
{
  Aabb2 box {{10.0f, 20.0f}, {50.0f, 60.0f}};
  
  REQUIRE(box.Min.X == 10.0f);
  REQUIRE(box.Min.Y == 20.0f);
  REQUIRE(box.Max.X == 50.0f);
  REQUIRE(box.Max.Y == 60.0f);
}

TEST_CASE("Aabb2 Size", "[aabb][Aabb2]")
{
  Aabb2 box {{10.0f, 20.0f}, {50.0f, 60.0f}};
  Float2 size = Size(box);
  
  REQUIRE(size.X == 40.0f);
  REQUIRE(size.Y == 40.0f);
}

TEST_CASE("Aabb2 Center", "[aabb][Aabb2]")
{
  Aabb2 box {{10.0f, 20.0f}, {50.0f, 60.0f}};
  Float2 center = Center(box);
  
  REQUIRE(center.X == 30.0f);
  REQUIRE(center.Y == 40.0f);
}

TEST_CASE("Aabb2 Contains", "[aabb][Aabb2]")
{
  Aabb2 box {{10.0f, 10.0f}, {50.0f, 50.0f}};
  
  SECTION("Point inside")
  {
    REQUIRE(Contains(box, {30.0f, 30.0f}));
  }
  
  SECTION("Point outside")
  {
    REQUIRE_FALSE(Contains(box, {5.0f, 30.0f}));
    REQUIRE_FALSE(Contains(box, {30.0f, 5.0f}));
    REQUIRE_FALSE(Contains(box, {60.0f, 30.0f}));
    REQUIRE_FALSE(Contains(box, {30.0f, 60.0f}));
  }
  
  SECTION("Point on edge")
  {
    REQUIRE(Contains(box, {10.0f, 30.0f}));
    REQUIRE(Contains(box, {50.0f, 30.0f}));
  }
}

TEST_CASE("Aabb2 Intersects", "[aabb][Aabb2]")
{
  Aabb2 box1 {{10.0f, 10.0f}, {50.0f, 50.0f}};
  
  SECTION("Overlapping boxes")
  {
    Aabb2 box2 {{30.0f, 30.0f}, {70.0f, 70.0f}};
    REQUIRE(Intersects(box1, box2));
  }
  
  SECTION("Non-overlapping boxes")
  {
    Aabb2 box2 {{60.0f, 60.0f}, {100.0f, 100.0f}};
    REQUIRE_FALSE(Intersects(box1, box2));
  }
  
  SECTION("Touching boxes")
  {
    Aabb2 box2 {{50.0f, 10.0f}, {90.0f, 50.0f}};
    REQUIRE(Intersects(box1, box2));
  }
}

TEST_CASE("Aabb2 Expand by scalar", "[aabb][Aabb2]")
{
  Aabb2 box {{10.0f, 10.0f}, {50.0f, 50.0f}};
  Aabb2 expanded = Expand(box, 5.0f);
  
  REQUIRE(expanded.Min.X == 5.0f);
  REQUIRE(expanded.Min.Y == 5.0f);
  REQUIRE(expanded.Max.X == 55.0f);
  REQUIRE(expanded.Max.Y == 55.0f);
}

TEST_CASE("Aabb2 Expand by point", "[aabb][Aabb2]")
{
  Aabb2 box {{10.0f, 10.0f}, {50.0f, 50.0f}};
  
  SECTION("Point outside expands box")
  {
    Aabb2 expanded = Expand(box, {60.0f, 70.0f});
    REQUIRE(expanded.Min.X == 10.0f);
    REQUIRE(expanded.Min.Y == 10.0f);
    REQUIRE(expanded.Max.X == 60.0f);
    REQUIRE(expanded.Max.Y == 70.0f);
  }
  
  SECTION("Point inside doesn't shrink box")
  {
    Aabb2 expanded = Expand(box, {30.0f, 30.0f});
    REQUIRE(expanded.Min.X == 10.0f);
    REQUIRE(expanded.Min.Y == 10.0f);
    REQUIRE(expanded.Max.X == 50.0f);
    REQUIRE(expanded.Max.Y == 50.0f);
  }
  
  SECTION("Point before min expands backwards")
  {
    Aabb2 expanded = Expand(box, {5.0f, 5.0f});
    REQUIRE(expanded.Min.X == 5.0f);
    REQUIRE(expanded.Min.Y == 5.0f);
    REQUIRE(expanded.Max.X == 50.0f);
    REQUIRE(expanded.Max.Y == 50.0f);
  }
}

TEST_CASE("Aabb2 Union", "[aabb][Aabb2]")
{
  Aabb2 box1 {{10.0f, 10.0f}, {50.0f, 50.0f}};
  Aabb2 box2 {{30.0f, 30.0f}, {70.0f, 70.0f}};
  
  Aabb2 combined = Union(box1, box2);
  
  REQUIRE(combined.Min.X == 10.0f);
  REQUIRE(combined.Min.Y == 10.0f);
  REQUIRE(combined.Max.X == 70.0f);
  REQUIRE(combined.Max.Y == 70.0f);
}

TEST_CASE("Aabb2 Clamp point", "[aabb][Aabb2]")
{
  Aabb2 box {{10.0f, 10.0f}, {50.0f, 50.0f}};
  
  SECTION("Point inside stays same")
  {
    Float2 clamped = Clamp(box, {30.0f, 30.0f});
    REQUIRE(clamped.X == 30.0f);
    REQUIRE(clamped.Y == 30.0f);
  }
  
  SECTION("Point outside gets clamped")
  {
    Float2 clamped = Clamp(box, {5.0f, 100.0f});
    REQUIRE(clamped.X == 10.0f);
    REQUIRE(clamped.Y == 50.0f);
  }
}

TEST_CASE("Aabb2i integer operations", "[aabb][Aabb2i]")
{
  Aabb2i box {{10, 10}, {50, 50}};
  
  SECTION("Size")
  {
    Int2 size = Size(box);
    REQUIRE(size.X == 40);
    REQUIRE(size.Y == 40);
  }
  
  SECTION("Center")
  {
    Int2 center = Center(box);
    REQUIRE(center.X == 30);
    REQUIRE(center.Y == 30);
  }
  
  SECTION("Contains")
  {
    REQUIRE(Contains(box, {30, 30}));
    REQUIRE_FALSE(Contains(box, {5, 30}));
  }
  
  SECTION("Intersects")
  {
    Aabb2i box2 {{30, 30}, {70, 70}};
    REQUIRE(Intersects(box, box2));
  }
  
  SECTION("Expand by scalar")
  {
    Aabb2i expanded = Expand(box, 5);
    REQUIRE(expanded.Min.X == 5);
    REQUIRE(expanded.Min.Y == 5);
    REQUIRE(expanded.Max.X == 55);
    REQUIRE(expanded.Max.Y == 55);
  }
  
  SECTION("Expand by point")
  {
    Aabb2i expanded = Expand(box, {60, 70});
    REQUIRE(expanded.Min.X == 10);
    REQUIRE(expanded.Min.Y == 10);
    REQUIRE(expanded.Max.X == 60);
    REQUIRE(expanded.Max.Y == 70);
  }
  
  SECTION("Union")
  {
    Aabb2i box2 {{30, 30}, {70, 70}};
    Aabb2i combined = Union(box, box2);
    REQUIRE(combined.Min.X == 10);
    REQUIRE(combined.Min.Y == 10);
    REQUIRE(combined.Max.X == 70);
    REQUIRE(combined.Max.Y == 70);
  }
  
  SECTION("Clamp")
  {
    Int2 clamped = Clamp(box, {5, 100});
    REQUIRE(clamped.X == 10);
    REQUIRE(clamped.Y == 50);
  }
}

TEST_CASE("Aabb2 array access", "[aabb][Aabb2]")
{
  Aabb2 box {{10.0f, 20.0f}, {50.0f, 60.0f}};
  
  REQUIRE(box[0].X == 10.0f);
  REQUIRE(box[0].Y == 20.0f);
  REQUIRE(box[1].X == 50.0f);
  REQUIRE(box[1].Y == 60.0f);
}
