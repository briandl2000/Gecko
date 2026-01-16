#pragma once

#include "gecko/core/api.h"
#include "gecko/core/types.h"

namespace gecko {

// Get current time in nanoseconds since epoch using steady_clock (monotonic)
// This is the recommended function for profiling and time measurements
GECKO_API u64 MonotonicTimeNs() noexcept;

// Get current time in nanoseconds since epoch using high_resolution_clock
// Use this for high precision timing requirements
GECKO_API u64 HighResTimeNs() noexcept;

// Get current system time in nanoseconds since Unix epoch
// Use this for timestamping events that need to correlate with system time
GECKO_API u64 SystemTimeNs() noexcept;

// Time conversion utilities
namespace time {

// Convert time units to nanoseconds
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

// Convert nanoseconds to other time units
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

// Convert nanoseconds to floating point time units
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
