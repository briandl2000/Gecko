#include "gecko/core/labels.h"

#include <catch2/catch_test_macros.hpp>

using namespace gecko;

TEST_CASE("Label default construction", "[core][labels]")
{
  Label label {};
  REQUIRE_FALSE(label.IsValid());
  REQUIRE(label.Id == 0);
  REQUIRE(label.Name == nullptr);
}

TEST_CASE("MakeLabel creates valid labels", "[core][labels]")
{
  Label label = MakeLabel("test.label");
  REQUIRE(label.IsValid());
  REQUIRE(label.Id != 0);
  REQUIRE(label.Name != nullptr);
}

TEST_CASE("MakeLabel with null/empty returns invalid", "[core][labels]")
{
  REQUIRE_FALSE(MakeLabel(nullptr).IsValid());
  REQUIRE_FALSE(MakeLabel("").IsValid());
}

TEST_CASE("Label equality", "[core][labels]")
{
  Label a = MakeLabel("same.name");
  Label b = MakeLabel("same.name");
  Label c = MakeLabel("different.name");

  REQUIRE(a == b);
  REQUIRE(a != c);
}

TEST_CASE("MakeLabel is constexpr", "[core][labels]")
{
  constexpr Label label = MakeLabel("constexpr.label");
  static_assert(label.IsValid());
  static_assert(label.Id != 0);
  REQUIRE(label.IsValid());
}

TEST_CASE("Different label strings produce different IDs", "[core][labels]")
{
  Label a = MakeLabel("gecko.core");
  Label b = MakeLabel("gecko.runtime");
  Label c = MakeLabel("gecko.platform");

  REQUIRE(a.Id != b.Id);
  REQUIRE(b.Id != c.Id);
  REQUIRE(a.Id != c.Id);
}
