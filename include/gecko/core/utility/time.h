#pragma once

/// @file
/// High-precision time queries and unit-conversion helpers.

#include "gecko/core/api.h"
#include "gecko/core/types.h"

namespace gecko {

/// @return Nanoseconds from a monotonic clock; suitable for measuring
///         intervals. Origin is unspecified.
GECKO_API u64 MonotonicTimeNs() noexcept;

/// @return Highest-resolution timestamp the platform exposes.
///         May coincide with `MonotonicTimeNs`.
GECKO_API u64 HighResTimeNs() noexcept;

/// @return Wall-clock time in nanoseconds since the Unix epoch.
GECKO_API u64 SystemTimeNs() noexcept;

namespace time {

/// Convert milliseconds to nanoseconds.
constexpr u64 MillisecondsToNs(u64 milliseconds) noexcept
{
  return milliseconds * 1'000'000ULL;
}

/// Convert microseconds to nanoseconds.
constexpr u64 MicrosecondsToNs(u64 microseconds) noexcept
{
  return microseconds * 1'000ULL;
}

/// Convert seconds to nanoseconds.
constexpr u64 SecondsToNs(u64 seconds) noexcept
{
  return seconds * 1'000'000'000ULL;
}

/// Truncating conversion from nanoseconds to microseconds.
constexpr u64 NsToMicroseconds(u64 nanoseconds) noexcept
{
  return nanoseconds / 1'000ULL;
}

/// Truncating conversion from nanoseconds to milliseconds.
constexpr u64 NsToMilliseconds(u64 nanoseconds) noexcept
{
  return nanoseconds / 1'000'000ULL;
}

/// Truncating conversion from nanoseconds to seconds.
constexpr u64 NsToSeconds(u64 nanoseconds) noexcept
{
  return nanoseconds / 1'000'000'000ULL;
}

/// Floating-point conversion from nanoseconds to microseconds.
constexpr double NsToMicrosecondsF(u64 nanoseconds) noexcept
{
  return static_cast<double>(nanoseconds) / 1'000.0;
}

/// Floating-point conversion from nanoseconds to milliseconds.
constexpr double NsToMillisecondsF(u64 nanoseconds) noexcept
{
  return static_cast<double>(nanoseconds) / 1'000'000.0;
}

/// Floating-point conversion from nanoseconds to seconds.
constexpr double NsToSecondsF(u64 nanoseconds) noexcept
{
  return static_cast<double>(nanoseconds) / 1'000'000'000.0;
}

}  // namespace time

}  // namespace gecko
