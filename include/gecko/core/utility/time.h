#pragma once

#include "gecko/core/api.h"
#include "gecko/core/types.h"

namespace gecko {

GECKO_API u64 MonotonicTimeNs() noexcept;

GECKO_API u64 HighResTimeNs() noexcept;

GECKO_API u64 SystemTimeNs() noexcept;

namespace time {

constexpr u64 MillisecondsToNs(u64 milliseconds) noexcept
{
  return milliseconds * 1'000'000ULL;
}

constexpr u64 MicrosecondsToNs(u64 microseconds) noexcept
{
  return microseconds * 1'000ULL;
}

constexpr u64 SecondsToNs(u64 seconds) noexcept
{
  return seconds * 1'000'000'000ULL;
}

constexpr u64 NsToMicroseconds(u64 nanoseconds) noexcept
{
  return nanoseconds / 1'000ULL;
}

constexpr u64 NsToMilliseconds(u64 nanoseconds) noexcept
{
  return nanoseconds / 1'000'000ULL;
}

constexpr u64 NsToSeconds(u64 nanoseconds) noexcept
{
  return nanoseconds / 1'000'000'000ULL;
}

constexpr double NsToMicrosecondsF(u64 nanoseconds) noexcept
{
  return static_cast<double>(nanoseconds) / 1'000.0;
}

constexpr double NsToMillisecondsF(u64 nanoseconds) noexcept
{
  return static_cast<double>(nanoseconds) / 1'000'000.0;
}

constexpr double NsToSecondsF(u64 nanoseconds) noexcept
{
  return static_cast<double>(nanoseconds) / 1'000'000'000.0;
}

}  // namespace time

}  // namespace gecko
