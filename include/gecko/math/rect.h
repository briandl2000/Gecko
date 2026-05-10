#pragma once

/// @file
/// Integer rectangle defined by a position and a size.
///
/// `Rect2D` mirrors the convention used by windowing APIs: the rectangle
/// covers pixels in the half-open range `[Position, Position + Size)`.
/// For a min/max representation use `Aabb2i` instead, or convert with
/// `ToAabb2i()`.

#include "gecko/math/aabb.h"
#include "gecko/math/vector.h"

namespace gecko::math {

struct Aabb2i;

/// Axis-aligned 2D rectangle in pixel space (position + size).
/// Treated as the half-open region `[Position, Position + Size)`.
struct Rect2D
{
  Int2 Position {};
  Int2 Size {};

  constexpr Rect2D() noexcept = default;

  constexpr Rect2D(Int2 position, Int2 size) noexcept : Position(position), Size(size)
  {}

  constexpr Rect2D(i32 x, i32 y, i32 width, i32 height) noexcept : Position(x, y), Size(width, height)
  {}

  [[nodiscard]] constexpr i32 X() const noexcept
  {
    return Position.X;
  }
  [[nodiscard]] constexpr i32 Y() const noexcept
  {
    return Position.Y;
  }
  [[nodiscard]] constexpr i32 Width() const noexcept
  {
    return Size.X;
  }
  [[nodiscard]] constexpr i32 Height() const noexcept
  {
    return Size.Y;
  }

  [[nodiscard]] constexpr i32 Right() const noexcept
  {
    return Position.X + Size.X;
  }

  [[nodiscard]] constexpr i32 Bottom() const noexcept
  {
    return Position.Y + Size.Y;
  }

  [[nodiscard]] constexpr Int2 Center() const noexcept
  {
    return {Position.X + Size.X / 2, Position.Y + Size.Y / 2};
  }

  [[nodiscard]] constexpr bool IsEmpty() const noexcept
  {
    return Size.X <= 0 || Size.Y <= 0;
  }

  [[nodiscard]] constexpr bool Contains(Int2 p) const noexcept
  {
    return p.X >= Position.X && p.Y >= Position.Y && p.X < Right() && p.Y < Bottom();
  }

  [[nodiscard]] constexpr bool Intersects(const Rect2D& other) const noexcept
  {
    return Position.X < other.Right() && Right() > other.Position.X && Position.Y < other.Bottom() &&
           Bottom() > other.Position.Y;
  }

  [[nodiscard]] constexpr Aabb2i ToAabb2i() const noexcept
  {
    return {Position, {Position.X + Size.X, Position.Y + Size.Y}};
  }

  [[nodiscard]] constexpr bool operator==(const Rect2D& other) const noexcept
  {
    return Position == other.Position && Size == other.Size;
  }

  [[nodiscard]] constexpr bool operator!=(const Rect2D& other) const noexcept
  {
    return !(*this == other);
  }
};

}  // namespace gecko::math
