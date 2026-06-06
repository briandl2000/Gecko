#pragma once

/// @file
/// Scalar math constants and helpers.

#include "gecko/core/types.h"

#include <cmath>

namespace gecko::math {

/// Mathematical pi as `f32`.
inline constexpr f32 Pi = 3.14159265358979323846f;
/// 2*pi (one full turn in radians).
inline constexpr f32 TwoPi = 6.28318530717958647692f;
/// pi/2 (a quarter turn in radians).
inline constexpr f32 HalfPi = 1.57079632679489661923f;
/// Default tolerance for approximate equality of `f32` values.
inline constexpr f32 Epsilon = 1e-6f;

/// Convert degrees to radians.
[[nodiscard]] constexpr f32 ToRadians(f32 degrees) noexcept
{
  return degrees * (Pi / 180.0f);
}

/// Convert radians to degrees.
[[nodiscard]] constexpr f32 ToDegrees(f32 radians) noexcept
{
  return radians * (180.0f / Pi);
}

/// Smaller of two `f32` values.
[[nodiscard]] constexpr f32 Min(f32 a, f32 b) noexcept
{
  return a < b ? a : b;
}

/// Larger of two `f32` values.
[[nodiscard]] constexpr f32 Max(f32 a, f32 b) noexcept
{
  return a > b ? a : b;
}

/// Clamp `value` into the closed interval `[min, max]`.
[[nodiscard]] constexpr f32 Clamp(f32 value, f32 min, f32 max) noexcept
{
  return value < min ? min : (value > max ? max : value);
}

/// Clamp `value` into `[0, 1]`.
[[nodiscard]] constexpr f32 Saturate(f32 value) noexcept
{
  return Clamp(value, 0.0f, 1.0f);
}

/// True when `a` and `b` differ by no more than `epsilon`.
[[nodiscard]] constexpr bool IsNearlyEqual(f32 a, f32 b, f32 epsilon = Epsilon) noexcept
{
  const f32 diff = a - b;
  return (diff < 0.0f ? -diff : diff) <= epsilon;
}

/// True when `value` is within `epsilon` of zero.
[[nodiscard]] constexpr bool IsNearlyZero(f32 value, f32 epsilon = Epsilon) noexcept
{
  return (value < 0.0f ? -value : value) <= epsilon;
}

/// Sign of `value`: -1 for negative, 1 for positive and 0 for zero.
[[nodiscard]] constexpr f32 Sign(f32 value) noexcept
{
  return value < 0.0f ? -1.0f : (value > 0.0f ? 1.0f : 0.0f);
}

/// Linear interpolation: `a + (b - a) * t`. `t` is not clamped.
[[nodiscard]] constexpr f32 Lerp(f32 a, f32 b, f32 t) noexcept
{
  return a + (b - a) * t;
}

/// Hermite smoothstep: 0 below `edge0`, 1 above `edge1`, smooth between.
[[nodiscard]] constexpr f32 Smoothstep(f32 edge0, f32 edge1, f32 x) noexcept
{
  const f32 t = Clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

/// Absolute value of `x`.
[[nodiscard]] constexpr f32 Abs(f32 x) noexcept
{
  return x < 0.0f ? -x : x;
}

/// Absolute value of `x`.
[[nodiscard]] constexpr i32 Abs(i32 x) noexcept
{
  return x < 0 ? -x : x;
}

/// Square root (wraps `std::sqrt`).
[[nodiscard]] inline f32 Sqrt(f32 x) noexcept
{
  return ::std::sqrt(x);
}

/// Floor (wraps `std::floor`).
[[nodiscard]] inline f32 Floor(f32 x) noexcept
{
  return ::std::floor(x);
}

/// Ceiling (wraps `std::ceil`).
[[nodiscard]] inline f32 Ceil(f32 x) noexcept
{
  return ::std::ceil(x);
}

/// Round to nearest integer value as `f32` (wraps `std::round`).
[[nodiscard]] inline f32 Round(f32 x) noexcept
{
  return ::std::round(x);
}

/// Fractional part of `x`.
[[nodiscard]] inline f32 Fract(f32 x) noexcept
{
  return x - Floor(x);
}

/// Floating-point modulo (wraps `std::fmod`).
[[nodiscard]] inline f32 Mod(f32 x, f32 y) noexcept
{
  return ::std::fmod(x, y);
}

/// Power (wraps `std::pow`).
[[nodiscard]] inline f32 Pow(f32 x, f32 y) noexcept
{
  return ::std::pow(x, y);
}

/// Sine of `x` (radians).
[[nodiscard]] inline f32 Sin(f32 x) noexcept
{
  return ::std::sin(x);
}

/// Cosine of `x` (radians).
[[nodiscard]] inline f32 Cos(f32 x) noexcept
{
  return ::std::cos(x);
}

/// Tangent of `x` (radians).
[[nodiscard]] inline f32 Tan(f32 x) noexcept
{
  return ::std::tan(x);
}

/// Two-argument arctangent: angle of `(x, y)` in radians.
[[nodiscard]] inline f32 Atan2(f32 y, f32 x) noexcept
{
  return ::std::atan2(y, x);
}

}  // namespace gecko::math
