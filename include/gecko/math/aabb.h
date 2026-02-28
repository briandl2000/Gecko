#pragma once

#include "gecko/math/vector.h"

namespace gecko::math {

struct Aabb2
{
  Float2 Min {};
  Float2 Max {};

  constexpr Aabb2() noexcept = default;
  constexpr Aabb2(const Float2& min, const Float2& max) noexcept
      : Min(min), Max(max)
  {}

  [[nodiscard]] constexpr Float2& operator[](usize index) noexcept
  {
    return (&Min)[index];
  }

  [[nodiscard]] constexpr const Float2& operator[](usize index) const noexcept
  {
    return (&Min)[index];
  }
};

struct Aabb2i
{
  Int2 Min {};
  Int2 Max {};

  constexpr Aabb2i() noexcept = default;
  constexpr Aabb2i(const Int2& min, const Int2& max) noexcept
      : Min(min), Max(max)
  {}

  [[nodiscard]] constexpr Int2& operator[](usize index) noexcept
  {
    return (&Min)[index];
  }

  [[nodiscard]] constexpr const Int2& operator[](usize index) const noexcept
  {
    return (&Min)[index];
  }
};

[[nodiscard]] constexpr Float2 Size(const Aabb2& box) noexcept
{
  return {box.Max.X - box.Min.X, box.Max.Y - box.Min.Y};
}

[[nodiscard]] constexpr Float2 Center(const Aabb2& box) noexcept
{
  return {(box.Min.X + box.Max.X) * 0.5f, (box.Min.Y + box.Max.Y) * 0.5f};
}

[[nodiscard]] constexpr bool Contains(const Aabb2& box,
                                      const Float2& p) noexcept
{
  return p.X >= box.Min.X && p.Y >= box.Min.Y && p.X <= box.Max.X &&
         p.Y <= box.Max.Y;
}

[[nodiscard]] constexpr bool Intersects(const Aabb2& a, const Aabb2& b) noexcept
{
  return !(b.Max.X < a.Min.X || b.Min.X > a.Max.X || b.Max.Y < a.Min.Y ||
           b.Min.Y > a.Max.Y);
}

[[nodiscard]] constexpr Aabb2 Expand(const Aabb2& box, f32 amount) noexcept
{
  return {box.Min - Float2 {amount, amount}, box.Max + Float2 {amount, amount}};
}

[[nodiscard]] constexpr Aabb2 Expand(const Aabb2& box, const Float2& p) noexcept
{
  return {::gecko::math::Min(box.Min, p), ::gecko::math::Max(box.Max, p)};
}

[[nodiscard]] constexpr Aabb2 Union(const Aabb2& a, const Aabb2& b) noexcept
{
  return {::gecko::math::Min(a.Min, b.Min), ::gecko::math::Max(a.Max, b.Max)};
}

[[nodiscard]] constexpr Float2 Clamp(const Aabb2& box, const Float2& p) noexcept
{
  return ::gecko::math::Clamp(p, box.Min, box.Max);
}

[[nodiscard]] constexpr Int2 Size(const Aabb2i& box) noexcept
{
  return {box.Max.X - box.Min.X, box.Max.Y - box.Min.Y};
}

[[nodiscard]] constexpr Int2 Center(const Aabb2i& box) noexcept
{
  return {(box.Min.X + box.Max.X) / 2, (box.Min.Y + box.Max.Y) / 2};
}

[[nodiscard]] constexpr bool Contains(const Aabb2i& box, const Int2& p) noexcept
{
  return p.X >= box.Min.X && p.Y >= box.Min.Y && p.X <= box.Max.X &&
         p.Y <= box.Max.Y;
}

[[nodiscard]] constexpr bool Intersects(const Aabb2i& a,
                                        const Aabb2i& b) noexcept
{
  return !(b.Max.X < a.Min.X || b.Min.X > a.Max.X || b.Max.Y < a.Min.Y ||
           b.Min.Y > a.Max.Y);
}

[[nodiscard]] constexpr Aabb2i Expand(const Aabb2i& box, i32 amount) noexcept
{
  return {box.Min - Int2 {amount, amount}, box.Max + Int2 {amount, amount}};
}

[[nodiscard]] constexpr Aabb2i Expand(const Aabb2i& box, const Int2& p) noexcept
{
  return {::gecko::math::Min(box.Min, p), ::gecko::math::Max(box.Max, p)};
}

[[nodiscard]] constexpr Aabb2i Union(const Aabb2i& a, const Aabb2i& b) noexcept
{
  return {::gecko::math::Min(a.Min, b.Min), ::gecko::math::Max(a.Max, b.Max)};
}

[[nodiscard]] constexpr Int2 Clamp(const Aabb2i& box, const Int2& p) noexcept
{
  return ::gecko::math::Clamp(p, box.Min, box.Max);
}

using RectF = Aabb2;
using RectI = Aabb2i;

}  // namespace gecko::math
