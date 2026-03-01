#include "gecko/core/version.h"

#include <catch2/catch_test_macros.hpp>
#include <cstring>

using namespace gecko;

TEST_CASE("Version numbers are non-negative", "[core][version]")
{
  REQUIRE(VersionMajor() >= 0);
  REQUIRE(VersionMinor() >= 0);
  REQUIRE(VersionPatch() >= 0);
}

TEST_CASE("VersionString is not empty", "[core][version]")
{
  const char* v = VersionString();
  REQUIRE(v != nullptr);
  REQUIRE(::std::strlen(v) > 0);
}

TEST_CASE("VersionFullString is not empty", "[core][version]")
{
  const char* v = VersionFullString();
  REQUIRE(v != nullptr);
  REQUIRE(::std::strlen(v) > 0);
}

TEST_CASE("VersionPrerelease returns a string", "[core][version]")
{
  const char* pre = VersionPrerelease();
  REQUIRE(pre != nullptr);
}
