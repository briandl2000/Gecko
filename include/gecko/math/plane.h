#pragma once

/// @file
/// 3D plane primitive and helpers.

#include "gecko/math/vector.h"

namespace gecko::math {

/// Plane represented by `Dot(Normal, point) + D == 0`.
struct Plane
{
  Float3 Normal {0.0f, 1.0f, 0.0f};
  f32 D {0.0f};

  constexpr Plane() noexcept = default;
  constexpr Plane(const Float3& normal, f32 d) noexcept : Normal(normal), D(d)
  {}

  [[nodiscard]] static inline Plane FromPointNormal(const Float3& point, const Float3& normal) noexcept
  {
    const Float3 n = Normalized(normal);
    return {n, -Dot(n, point)};
  }

  [[nodiscard]] static inline Plane FromTriangle(const Float3& a, const Float3& b, const Float3& c) noexcept
  {
    const Float3 n = Normalized(Cross(b - a, c - a));
    return {n, -Dot(n, a)};
  }
};

[[nodiscard]] inline Plane Normalized(const Plane& p) noexcept
{
  const f32 len = Length(p.Normal);
  return len > Epsilon ? Plane {p.Normal / len, p.D / len} : Plane {};
}

[[nodiscard]] constexpr f32 Distance(const Plane& p, const Float3& point) noexcept
{
  return Dot(p.Normal, point) + p.D;
}

[[nodiscard]] constexpr Float3 ProjectPoint(const Plane& p, const Float3& point) noexcept
{
  return point - p.Normal * Distance(p, point);
}

}  // namespace gecko::math
