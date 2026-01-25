#pragma once

#include "gecko/core/types.h"

#include <cmath>

namespace gecko::math {

struct Float2
{
  union
  {
    struct
    {
      f32 X;
      f32 Y;
    };
    f32 Data[2];
  };

  constexpr Float2() noexcept : X(0.0f), Y(0.0f)
  {}
  constexpr Float2(f32 x, f32 y) noexcept : X(x), Y(y)
  {}

  [[nodiscard]] constexpr f32& operator[](usize index) noexcept
  {
    return Data[index];
  }

  [[nodiscard]] constexpr const f32& operator[](usize index) const noexcept
  {
    return Data[index];
  }
};

struct Float3
{
  union
  {
    struct
    {
      f32 X;
      f32 Y;
      f32 Z;
    };
    f32 Data[3];
  };

  constexpr Float3() noexcept : X(0.0f), Y(0.0f), Z(0.0f)
  {}
  constexpr Float3(f32 x, f32 y, f32 z) noexcept : X(x), Y(y), Z(z)
  {}

  [[nodiscard]] constexpr f32& operator[](usize index) noexcept
  {
    return Data[index];
  }

  [[nodiscard]] constexpr const f32& operator[](usize index) const noexcept
  {
    return Data[index];
  }
};

struct Float4
{
  union
  {
    struct
    {
      f32 X;
      f32 Y;
      f32 Z;
      f32 W;
    };
    f32 Data[4];
  };

  constexpr Float4() noexcept : X(0.0f), Y(0.0f), Z(0.0f), W(0.0f)
  {}
  constexpr Float4(f32 x, f32 y, f32 z, f32 w) noexcept : X(x), Y(y), Z(z), W(w)
  {}

  [[nodiscard]] constexpr f32& operator[](usize index) noexcept
  {
    return Data[index];
  }

  [[nodiscard]] constexpr const f32& operator[](usize index) const noexcept
  {
    return Data[index];
  }
};

struct Int2
{
  union
  {
    struct
    {
      i32 X;
      i32 Y;
    };
    i32 Data[2];
  };

  constexpr Int2() noexcept : X(0), Y(0)
  {}
  constexpr Int2(i32 x, i32 y) noexcept : X(x), Y(y)
  {}

  [[nodiscard]] constexpr i32& operator[](usize index) noexcept
  {
    return Data[index];
  }

  [[nodiscard]] constexpr const i32& operator[](usize index) const noexcept
  {
    return Data[index];
  }
};

struct Int3
{
  union
  {
    struct
    {
      i32 X;
      i32 Y;
      i32 Z;
    };
    i32 Data[3];
  };

  constexpr Int3() noexcept : X(0), Y(0), Z(0)
  {}
  constexpr Int3(i32 x, i32 y, i32 z) noexcept : X(x), Y(y), Z(z)
  {}

  [[nodiscard]] constexpr i32& operator[](usize index) noexcept
  {
    return Data[index];
  }

  [[nodiscard]] constexpr const i32& operator[](usize index) const noexcept
  {
    return Data[index];
  }
};

struct Int4
{
  union
  {
    struct
    {
      i32 X;
      i32 Y;
      i32 Z;
      i32 W;
    };
    i32 Data[4];
  };

  constexpr Int4() noexcept : X(0), Y(0), Z(0), W(0)
  {}
  constexpr Int4(i32 x, i32 y, i32 z, i32 w) noexcept : X(x), Y(y), Z(z), W(w)
  {}

  [[nodiscard]] constexpr i32& operator[](usize index) noexcept
  {
    return Data[index];
  }

  [[nodiscard]] constexpr const i32& operator[](usize index) const noexcept
  {
    return Data[index];
  }
};

// Float2 operations
constexpr bool operator==(const Float2& a, const Float2& b) noexcept
{
  return a.X == b.X && a.Y == b.Y;
}

constexpr bool operator!=(const Float2& a, const Float2& b) noexcept
{
  return !(a == b);
}

constexpr Float2 operator+(const Float2& a, const Float2& b) noexcept
{
  return {a.X + b.X, a.Y + b.Y};
}

constexpr Float2 operator-(const Float2& a, const Float2& b) noexcept
{
  return {a.X - b.X, a.Y - b.Y};
}

constexpr Float2 operator*(const Float2& v, f32 s) noexcept
{
  return {v.X * s, v.Y * s};
}

constexpr Float2 operator*(f32 s, const Float2& v) noexcept
{
  return {v.X * s, v.Y * s};
}

constexpr Float2 operator/(const Float2& v, f32 s) noexcept
{
  return {v.X / s, v.Y / s};
}

[[nodiscard]] constexpr f32 Dot(const Float2& a, const Float2& b) noexcept
{
  return a.X * b.X + a.Y * b.Y;
}

[[nodiscard]] constexpr f32 Cross(const Float2& a, const Float2& b) noexcept
{
  return a.X * b.Y - a.Y * b.X;
}

[[nodiscard]] constexpr f32 LengthSquared(const Float2& v) noexcept
{
  return v.X * v.X + v.Y * v.Y;
}

[[nodiscard]] inline f32 Length(const Float2& v) noexcept
{
  return ::std::sqrt(LengthSquared(v));
}

[[nodiscard]] inline Float2 Normalized(const Float2& v) noexcept
{
  const f32 len = Length(v);
  return len > 0.0f ? (v / len) : Float2 {};
}

// Float3 operations
constexpr bool operator==(const Float3& a, const Float3& b) noexcept
{
  return a.X == b.X && a.Y == b.Y && a.Z == b.Z;
}

constexpr bool operator!=(const Float3& a, const Float3& b) noexcept
{
  return !(a == b);
}

constexpr Float3 operator+(const Float3& a, const Float3& b) noexcept
{
  return {a.X + b.X, a.Y + b.Y, a.Z + b.Z};
}

constexpr Float3 operator-(const Float3& a, const Float3& b) noexcept
{
  return {a.X - b.X, a.Y - b.Y, a.Z - b.Z};
}

constexpr Float3 operator*(const Float3& v, f32 s) noexcept
{
  return {v.X * s, v.Y * s, v.Z * s};
}

constexpr Float3 operator*(f32 s, const Float3& v) noexcept
{
  return {v.X * s, v.Y * s, v.Z * s};
}

constexpr Float3 operator/(const Float3& v, f32 s) noexcept
{
  return {v.X / s, v.Y / s, v.Z / s};
}

[[nodiscard]] constexpr f32 Dot(const Float3& a, const Float3& b) noexcept
{
  return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
}

[[nodiscard]] constexpr Float3 Cross(const Float3& a, const Float3& b) noexcept
{
  return {a.Y * b.Z - a.Z * b.Y, a.Z * b.X - a.X * b.Z, a.X * b.Y - a.Y * b.X};
}

[[nodiscard]] constexpr f32 LengthSquared(const Float3& v) noexcept
{
  return v.X * v.X + v.Y * v.Y + v.Z * v.Z;
}

[[nodiscard]] inline f32 Length(const Float3& v) noexcept
{
  return ::std::sqrt(LengthSquared(v));
}

[[nodiscard]] inline Float3 Normalized(const Float3& v) noexcept
{
  const f32 len = Length(v);
  return len > 0.0f ? (v / len) : Float3 {};
}

// Float4 operations
constexpr bool operator==(const Float4& a, const Float4& b) noexcept
{
  return a.X == b.X && a.Y == b.Y && a.Z == b.Z && a.W == b.W;
}

constexpr bool operator!=(const Float4& a, const Float4& b) noexcept
{
  return !(a == b);
}

constexpr Float4 operator+(const Float4& a, const Float4& b) noexcept
{
  return {a.X + b.X, a.Y + b.Y, a.Z + b.Z, a.W + b.W};
}

constexpr Float4 operator-(const Float4& a, const Float4& b) noexcept
{
  return {a.X - b.X, a.Y - b.Y, a.Z - b.Z, a.W - b.W};
}

constexpr Float4 operator*(const Float4& v, f32 s) noexcept
{
  return {v.X * s, v.Y * s, v.Z * s, v.W * s};
}

constexpr Float4 operator*(f32 s, const Float4& v) noexcept
{
  return {v.X * s, v.Y * s, v.Z * s, v.W * s};
}

constexpr Float4 operator/(const Float4& v, f32 s) noexcept
{
  return {v.X / s, v.Y / s, v.Z / s, v.W / s};
}

[[nodiscard]] constexpr f32 Dot(const Float4& a, const Float4& b) noexcept
{
  return a.X * b.X + a.Y * b.Y + a.Z * b.Z + a.W * b.W;
}

[[nodiscard]] constexpr f32 LengthSquared(const Float4& v) noexcept
{
  return v.X * v.X + v.Y * v.Y + v.Z * v.Z + v.W * v.W;
}

[[nodiscard]] inline f32 Length(const Float4& v) noexcept
{
  return ::std::sqrt(LengthSquared(v));
}

[[nodiscard]] inline Float4 Normalized(const Float4& v) noexcept
{
  const f32 len = Length(v);
  return len > 0.0f ? (v / len) : Float4 {};
}

// Int2 operations
constexpr bool operator==(const Int2& a, const Int2& b) noexcept
{
  return a.X == b.X && a.Y == b.Y;
}

constexpr bool operator!=(const Int2& a, const Int2& b) noexcept
{
  return !(a == b);
}

constexpr Int2 operator+(const Int2& a, const Int2& b) noexcept
{
  return {a.X + b.X, a.Y + b.Y};
}

constexpr Int2 operator-(const Int2& a, const Int2& b) noexcept
{
  return {a.X - b.X, a.Y - b.Y};
}

constexpr Int2 operator*(const Int2& v, i32 s) noexcept
{
  return {v.X * s, v.Y * s};
}

constexpr Int2 operator/(const Int2& v, i32 s) noexcept
{
  return {v.X / s, v.Y / s};
}

[[nodiscard]] constexpr i32 Dot(const Int2& a, const Int2& b) noexcept
{
  return a.X * b.X + a.Y * b.Y;
}

// Int3 operations
constexpr bool operator==(const Int3& a, const Int3& b) noexcept
{
  return a.X == b.X && a.Y == b.Y && a.Z == b.Z;
}

constexpr bool operator!=(const Int3& a, const Int3& b) noexcept
{
  return !(a == b);
}

constexpr Int3 operator+(const Int3& a, const Int3& b) noexcept
{
  return {a.X + b.X, a.Y + b.Y, a.Z + b.Z};
}

constexpr Int3 operator-(const Int3& a, const Int3& b) noexcept
{
  return {a.X - b.X, a.Y - b.Y, a.Z - b.Z};
}

constexpr Int3 operator*(const Int3& v, i32 s) noexcept
{
  return {v.X * s, v.Y * s, v.Z * s};
}

constexpr Int3 operator/(const Int3& v, i32 s) noexcept
{
  return {v.X / s, v.Y / s, v.Z / s};
}

[[nodiscard]] constexpr i32 Dot(const Int3& a, const Int3& b) noexcept
{
  return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
}

// Int4 operations
constexpr bool operator==(const Int4& a, const Int4& b) noexcept
{
  return a.X == b.X && a.Y == b.Y && a.Z == b.Z && a.W == b.W;
}

constexpr bool operator!=(const Int4& a, const Int4& b) noexcept
{
  return !(a == b);
}

constexpr Int4 operator+(const Int4& a, const Int4& b) noexcept
{
  return {a.X + b.X, a.Y + b.Y, a.Z + b.Z, a.W + b.W};
}

constexpr Int4 operator-(const Int4& a, const Int4& b) noexcept
{
  return {a.X - b.X, a.Y - b.Y, a.Z - b.Z, a.W - b.W};
}

constexpr Int4 operator*(const Int4& v, i32 s) noexcept
{
  return {v.X * s, v.Y * s, v.Z * s, v.W * s};
}

constexpr Int4 operator/(const Int4& v, i32 s) noexcept
{
  return {v.X / s, v.Y / s, v.Z / s, v.W / s};
}

[[nodiscard]] constexpr i32 Dot(const Int4& a, const Int4& b) noexcept
{
  return a.X * b.X + a.Y * b.Y + a.Z * b.Z + a.W * b.W;
}

// Convenience aliases
using float2 = Float2;
using float3 = Float3;
using float4 = Float4;
using int2 = Int2;
using int3 = Int3;
using int4 = Int4;

using Point2 = Float2;
using Point3 = Float3;
using Point4 = Float4;
using Point2i = Int2;
using Point3i = Int3;
using Point4i = Int4;

using Size2 = Float2;
using Size3 = Float3;
using Size4 = Float4;
using Size2i = Int2;
using Size3i = Int3;
using Size4i = Int4;

}  // namespace gecko::math
