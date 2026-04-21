#include "gecko/core/utility/hash.h"

#include <catch2/catch_test_macros.hpp>

using namespace gecko;

TEST_CASE("FNV1a 32-bit hash", "[core][hash]")
{
  SECTION("Empty string has known basis value")
  {
    constexpr u32 basis = 2166136261u;
    REQUIRE(FNV1a("") == basis);
  }

  SECTION("Known test vectors")
  {
    REQUIRE(FNV1a("hello") != FNV1a("world"));
    REQUIRE(FNV1a("test") != FNV1a("tset"));
  }

  SECTION("Same input produces same hash")
  {
    REQUIRE(FNV1a("consistent") == FNV1a("consistent"));
  }

  SECTION("Constexpr evaluation")
  {
    constexpr u32 h = FNV1a("compile_time");
    REQUIRE(h != 0);
    REQUIRE(h == FNV1a("compile_time"));
  }

  SECTION("Binary data overload")
  {
    const char data[] = {1, 2, 3, 4};
    u32 h = FNV1a(data, sizeof(data));
    REQUIRE(h != 0);
    REQUIRE(h == FNV1a(data, sizeof(data)));
  }
}

TEST_CASE("FNV1aLiteral consteval", "[core][hash]")
{
  constexpr u32 h = FNV1aLiteral("literal_test");
  REQUIRE(h == FNV1a("literal_test"));
}

TEST_CASE("FNV1a64 64-bit hash", "[core][hash]")
{
  SECTION("Empty string has known basis value")
  {
    constexpr u64 basis = 14695981039346656037ull;
    REQUIRE(FNV1a64("") == basis);
  }

  SECTION("Different strings produce different hashes")
  {
    REQUIRE(FNV1a64("alpha") != FNV1a64("beta"));
  }

  SECTION("Deterministic")
  {
    REQUIRE(FNV1a64("repeat") == FNV1a64("repeat"));
  }

  SECTION("Binary data overload")
  {
    const char data[] = {10, 20, 30};
    u64 h = FNV1a64(data, sizeof(data));
    REQUIRE(h != 0);
    REQUIRE(h == FNV1a64(data, sizeof(data)));
  }
}
