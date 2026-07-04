#pragma once

/// @file
/// Rays and basic ray intersection helpers.

#include "gecko/math/aabb.h"
#include "gecko/math/plane.h"

namespace gecko::math {

struct Ray
{
  Float3 Origin {};
  Float3 Direction {0.0f, 0.0f, -1.0f};

  constexpr Ray() noexcept = default;
  constexpr Ray(const Float3& origin, const Float3& direction) noexcept : Origin(origin), Direction(direction)
  {}
};

[[nodiscard]] constexpr Float3 PointAt(const Ray& ray, f32 t) noexcept
{
  return ray.Origin + ray.Direction * t;
}

[[nodiscard]] inline bool IntersectRayPlane(const Ray& ray, const Plane& plane, f32& outT) noexcept
{
  const f32 denom = Dot(plane.Normal, ray.Direction);
  if (Abs(denom) <= Epsilon)
  {
    return false;
  }

  outT = -(Dot(plane.Normal, ray.Origin) + plane.D) / denom;
  return outT >= 0.0f;
}

[[nodiscard]] inline bool IntersectRayAabb(const Ray& ray, const Aabb3& box, f32& outTMin, f32& outTMax) noexcept
{
  f32 tMin = 0.0f;
  f32 tMax = 3.40282346638528859812e38f;

  for (usize i = 0; i < 3; ++i)
  {
    const f32 origin = ray.Origin[i];
    const f32 direction = ray.Direction[i];
    const f32 min = box.Min[i];
    const f32 max = box.Max[i];

    if (direction == 0.0f || Abs(direction) <= Epsilon)
    {
      if (origin < min || origin > max)
      {
        return false;
      }
      continue;
    }

    f32 t1 = (min - origin) / direction;
    f32 t2 = (max - origin) / direction;
    if (t1 > t2)
    {
      const f32 temp = t1;
      t1 = t2;
      t2 = temp;
    }

    tMin = Max(tMin, t1);
    tMax = Min(tMax, t2);
    if (tMin > tMax)
    {
      return false;
    }
  }

  outTMin = tMin;
  outTMax = tMax;
  return true;
}

}  // namespace gecko::math
