#include "gecko/core/utility/time.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

using namespace gecko;
using Catch::Matchers::WithinAbs;

TEST_CASE("Time conversion: seconds to nanoseconds", "[core][time]")
{
  REQUIRE(time::SecondsToNs(1) == 1'000'000'000ULL);
  REQUIRE(time::SecondsToNs(0) == 0);
  REQUIRE(time::SecondsToNs(5) == 5'000'000'000ULL);
}

TEST_CASE("Time conversion: milliseconds to nanoseconds", "[core][time]")
{
  REQUIRE(time::MillisecondsToNs(1) == 1'000'000ULL);
  REQUIRE(time::MillisecondsToNs(1000) == 1'000'000'000ULL);
}

TEST_CASE("Time conversion: microseconds to nanoseconds", "[core][time]")
{
  REQUIRE(time::MicrosecondsToNs(1) == 1'000ULL);
  REQUIRE(time::MicrosecondsToNs(1000) == 1'000'000ULL);
}

TEST_CASE("Time conversion: nanoseconds to larger units", "[core][time]")
{
  REQUIRE(time::NsToSeconds(1'000'000'000ULL) == 1);
  REQUIRE(time::NsToMilliseconds(1'000'000ULL) == 1);
  REQUIRE(time::NsToMicroseconds(1'000ULL) == 1);
}

TEST_CASE("Time conversion: floating point", "[core][time]")
{
  REQUIRE_THAT(time::NsToSecondsF(1'500'000'000ULL), WithinAbs(1.5, 0.001));
  REQUIRE_THAT(time::NsToMillisecondsF(1'500'000ULL), WithinAbs(1.5, 0.001));
  REQUIRE_THAT(time::NsToMicrosecondsF(1'500ULL), WithinAbs(1.5, 0.001));
}

TEST_CASE("Time conversion roundtrips", "[core][time]")
{
  constexpr u64 seconds = 42;
  REQUIRE(time::NsToSeconds(time::SecondsToNs(seconds)) == seconds);

  constexpr u64 ms = 1234;
  REQUIRE(time::NsToMilliseconds(time::MillisecondsToNs(ms)) == ms);

  constexpr u64 us = 56789;
  REQUIRE(time::NsToMicroseconds(time::MicrosecondsToNs(us)) == us);
}

TEST_CASE("MonotonicTimeNs is monotonic", "[core][time]")
{
  u64 t1 = MonotonicTimeNs();
  u64 t2 = MonotonicTimeNs();
  REQUIRE(t2 >= t1);
}

TEST_CASE("HighResTimeNs returns non-zero", "[core][time]")
{
  REQUIRE(HighResTimeNs() > 0);
}

TEST_CASE("SystemTimeNs returns non-zero", "[core][time]")
{
  REQUIRE(SystemTimeNs() > 0);
}
