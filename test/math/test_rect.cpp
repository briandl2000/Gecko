#include "gecko/math/aabb.h"
#include "gecko/math/rect.h"

#include <catch2/catch_test_macros.hpp>

using namespace gecko::math;

TEST_CASE("Rect2D default is zero", "[math][rect]")
{
  Rect2D r;
  REQUIRE(r.Position == Int2 {0, 0});
  REQUIRE(r.Size == Int2 {0, 0});
  REQUIRE(r.IsEmpty());
}

TEST_CASE("Rect2D position+size constructor", "[math][rect]")
{
  Rect2D r {Int2 {10, 20}, Int2 {100, 50}};
  REQUIRE(r.Position == Int2 {10, 20});
  REQUIRE(r.Size == Int2 {100, 50});
}

TEST_CASE("Rect2D flat (x,y,w,h) constructor", "[math][rect]")
{
  Rect2D r {5, 15, 80, 40};
  REQUIRE(r.X() == 5);
  REQUIRE(r.Y() == 15);
  REQUIRE(r.Width() == 80);
  REQUIRE(r.Height() == 40);
}

TEST_CASE("Rect2D Right and Bottom", "[math][rect]")
{
  Rect2D r {10, 20, 100, 50};
  REQUIRE(r.Right() == 110);
  REQUIRE(r.Bottom() == 70);
}

TEST_CASE("Rect2D Center", "[math][rect]")
{
  Rect2D r {10, 20, 100, 50};
  REQUIRE(r.Center() == Int2 {60, 45});
}

TEST_CASE("Rect2D negative origin (multi-monitor layout)", "[math][rect]")
{
  Rect2D r {-1920, 0, 1920, 1080};
  REQUIRE(r.X() == -1920);
  REQUIRE(r.Right() == 0);
  REQUIRE_FALSE(r.IsEmpty());
}

TEST_CASE("Rect2D Contains", "[math][rect]")
{
  Rect2D r {0, 0, 100, 100};
  REQUIRE(r.Contains({0, 0}));
  REQUIRE(r.Contains({50, 50}));
  REQUIRE(r.Contains({99, 99}));
  REQUIRE_FALSE(r.Contains({100, 100}));  // exclusive boundary
  REQUIRE_FALSE(r.Contains({-1, 0}));
}

TEST_CASE("Rect2D Intersects", "[math][rect]")
{
  Rect2D a {0, 0, 100, 100};
  Rect2D b {50, 50, 100, 100};
  Rect2D c {200, 200, 50, 50};

  REQUIRE(a.Intersects(b));
  REQUIRE(b.Intersects(a));
  REQUIRE_FALSE(a.Intersects(c));
}

TEST_CASE("Rect2D equality operators", "[math][rect]")
{
  Rect2D a {0, 0, 1920, 1080};
  Rect2D b {0, 0, 1920, 1080};
  Rect2D c {0, 0, 2560, 1440};

  REQUIRE(a == b);
  REQUIRE(a != c);
}

TEST_CASE("Rect2D ToAabb2i conversion", "[math][rect]")
{
  Rect2D r {10, 20, 100, 50};
  Aabb2i box = r.ToAabb2i();

  REQUIRE(box.Min == Int2 {10, 20});
  REQUIRE(box.Max == Int2 {110, 70});
}
