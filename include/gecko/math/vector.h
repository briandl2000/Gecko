#pragma once

/// @file
/// Fixed-size float and integer vectors plus vector math helpers.
///
/// Defines `Float2`/`Float3`/`Float4`, `Int2`/`Int3`/`Int4`. All vector
/// types expose both named members (`X`/`Y`/`Z`/`W`) and indexed access via
/// `Data[]` / `operator[]`. Scalar helpers are provided by
/// `gecko/math/scalar.h`, included here for compatibility and convenience.
///
/// Conventions: angles are in radians unless suffixed `Degrees`; the
/// coordinate system is right-handed.

#include "gecko/math/scalar.h"

namespace gecko::math {

/// Two-component float vector. Members `X`/`Y` alias `Data[0..1]`.
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

/// Three-component float vector. Members `X`/`Y`/`Z` alias `Data[0..2]`.
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

/// Four-component float vector (typically a homogeneous point or RGBA).
/// Members `X`/`Y`/`Z`/`W` alias `Data[0..3]`.
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

/// Two-component signed-int vector. Members `X`/`Y` alias `Data[0..1]`.
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

/// Three-component signed-int vector. Members `X`/`Y`/`Z` alias `Data[0..2]`.
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

/// Four-component signed-int vector. Members `X`/`Y`/`Z`/`W` alias
/// `Data[0..3]`.
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

constexpr Float2 operator-(const Float2& v) noexcept
{
  return {-v.X, -v.Y};
}

constexpr Float2 operator*(const Float2& a, const Float2& b) noexcept
{
  return {a.X * b.X, a.Y * b.Y};
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

constexpr Float2 operator/(const Float2& a, const Float2& b) noexcept
{
  return {a.X / b.X, a.Y / b.Y};
}

constexpr Float2& operator+=(Float2& a, const Float2& b) noexcept
{
  a = a + b;
  return a;
}

constexpr Float2& operator-=(Float2& a, const Float2& b) noexcept
{
  a = a - b;
  return a;
}

constexpr Float2& operator*=(Float2& a, const Float2& b) noexcept
{
  a = a * b;
  return a;
}

constexpr Float2& operator*=(Float2& v, f32 s) noexcept
{
  v = v * s;
  return v;
}

constexpr Float2& operator/=(Float2& a, const Float2& b) noexcept
{
  a = a / b;
  return a;
}

constexpr Float2& operator/=(Float2& v, f32 s) noexcept
{
  v = v / s;
  return v;
}

/// Dot (scalar) product of two vectors. Overloaded for every vector type.
[[nodiscard]] constexpr f32 Dot(const Float2& a, const Float2& b) noexcept
{
  return a.X * b.X + a.Y * b.Y;
}

/// Scalar 2D "cross" product: `a.X*b.Y - a.Y*b.X`. Sign indicates winding.
[[nodiscard]] constexpr f32 Cross(const Float2& a, const Float2& b) noexcept
{
  return a.X * b.Y - a.Y * b.X;
}

/// Squared length of a vector (avoids the sqrt). Overloaded per vector type.
[[nodiscard]] constexpr f32 LengthSquared(const Float2& v) noexcept
{
  return v.X * v.X + v.Y * v.Y;
}

/// Squared distance between two points/vectors.
[[nodiscard]] constexpr f32 DistanceSquared(const Float2& a, const Float2& b) noexcept
{
  return LengthSquared(a - b);
}

/// Euclidean length (magnitude) of a vector. Overloaded per vector type.
[[nodiscard]] inline f32 Length(const Float2& v) noexcept
{
  return ::std::sqrt(LengthSquared(v));
}

/// Euclidean distance between two points/vectors.
[[nodiscard]] inline f32 Distance(const Float2& a, const Float2& b) noexcept
{
  return Length(a - b);
}

/// Returns `v` scaled to unit length, or the zero vector when `Length(v) == 0`.
[[nodiscard]] inline Float2 Normalized(const Float2& v) noexcept
{
  const f32 len = Length(v);
  return len > 0.0f ? (v / len) : Float2 {};
}

[[nodiscard]] constexpr Float2 Min(const Float2& a, const Float2& b) noexcept
{
  return {Min(a.X, b.X), Min(a.Y, b.Y)};
}

[[nodiscard]] constexpr Float2 Max(const Float2& a, const Float2& b) noexcept
{
  return {Max(a.X, b.X), Max(a.Y, b.Y)};
}

[[nodiscard]] constexpr Float2 Clamp(const Float2& v, const Float2& min, const Float2& max) noexcept
{
  return {Clamp(v.X, min.X, max.X), Clamp(v.Y, min.Y, max.Y)};
}

[[nodiscard]] constexpr Float2 Lerp(const Float2& a, const Float2& b, f32 t) noexcept
{
  return {Lerp(a.X, b.X, t), Lerp(a.Y, b.Y, t)};
}

[[nodiscard]] constexpr Float2 Abs(const Float2& v) noexcept
{
  return {Abs(v.X), Abs(v.Y)};
}

/// Reflect `incident` around a unit surface `normal`.
[[nodiscard]] constexpr Float2 Reflect(const Float2& incident, const Float2& normal) noexcept
{
  return incident - normal * (2.0f * Dot(incident, normal));
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

constexpr Float3 operator-(const Float3& v) noexcept
{
  return {-v.X, -v.Y, -v.Z};
}

constexpr Float3 operator*(const Float3& a, const Float3& b) noexcept
{
  return {a.X * b.X, a.Y * b.Y, a.Z * b.Z};
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

constexpr Float3 operator/(const Float3& a, const Float3& b) noexcept
{
  return {a.X / b.X, a.Y / b.Y, a.Z / b.Z};
}

constexpr Float3& operator+=(Float3& a, const Float3& b) noexcept
{
  a = a + b;
  return a;
}

constexpr Float3& operator-=(Float3& a, const Float3& b) noexcept
{
  a = a - b;
  return a;
}

constexpr Float3& operator*=(Float3& a, const Float3& b) noexcept
{
  a = a * b;
  return a;
}

constexpr Float3& operator*=(Float3& v, f32 s) noexcept
{
  v = v * s;
  return v;
}

constexpr Float3& operator/=(Float3& a, const Float3& b) noexcept
{
  a = a / b;
  return a;
}

constexpr Float3& operator/=(Float3& v, f32 s) noexcept
{
  v = v / s;
  return v;
}

[[nodiscard]] constexpr f32 Dot(const Float3& a, const Float3& b) noexcept
{
  return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
}

/// 3D cross product: vector orthogonal to both `a` and `b`, right-handed.
[[nodiscard]] constexpr Float3 Cross(const Float3& a, const Float3& b) noexcept
{
  return {a.Y * b.Z - a.Z * b.Y, a.Z * b.X - a.X * b.Z, a.X * b.Y - a.Y * b.X};
}

[[nodiscard]] constexpr f32 LengthSquared(const Float3& v) noexcept
{
  return v.X * v.X + v.Y * v.Y + v.Z * v.Z;
}

[[nodiscard]] constexpr f32 DistanceSquared(const Float3& a, const Float3& b) noexcept
{
  return LengthSquared(a - b);
}

[[nodiscard]] inline f32 Length(const Float3& v) noexcept
{
  return ::std::sqrt(LengthSquared(v));
}

[[nodiscard]] inline f32 Distance(const Float3& a, const Float3& b) noexcept
{
  return Length(a - b);
}

[[nodiscard]] inline Float3 Normalized(const Float3& v) noexcept
{
  const f32 len = Length(v);
  return len > 0.0f ? (v / len) : Float3 {};
}

[[nodiscard]] constexpr Float3 Min(const Float3& a, const Float3& b) noexcept
{
  return {Min(a.X, b.X), Min(a.Y, b.Y), Min(a.Z, b.Z)};
}

[[nodiscard]] constexpr Float3 Max(const Float3& a, const Float3& b) noexcept
{
  return {Max(a.X, b.X), Max(a.Y, b.Y), Max(a.Z, b.Z)};
}

[[nodiscard]] constexpr Float3 Clamp(const Float3& v, const Float3& min, const Float3& max) noexcept
{
  return {Clamp(v.X, min.X, max.X), Clamp(v.Y, min.Y, max.Y), Clamp(v.Z, min.Z, max.Z)};
}

[[nodiscard]] constexpr Float3 Lerp(const Float3& a, const Float3& b, f32 t) noexcept
{
  return {Lerp(a.X, b.X, t), Lerp(a.Y, b.Y, t), Lerp(a.Z, b.Z, t)};
}

[[nodiscard]] constexpr Float3 Abs(const Float3& v) noexcept
{
  return {Abs(v.X), Abs(v.Y), Abs(v.Z)};
}

[[nodiscard]] constexpr Float3 Reflect(const Float3& incident, const Float3& normal) noexcept
{
  return incident - normal * (2.0f * Dot(incident, normal));
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

constexpr Float4 operator-(const Float4& v) noexcept
{
  return {-v.X, -v.Y, -v.Z, -v.W};
}

constexpr Float4 operator*(const Float4& a, const Float4& b) noexcept
{
  return {a.X * b.X, a.Y * b.Y, a.Z * b.Z, a.W * b.W};
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

constexpr Float4 operator/(const Float4& a, const Float4& b) noexcept
{
  return {a.X / b.X, a.Y / b.Y, a.Z / b.Z, a.W / b.W};
}

constexpr Float4& operator+=(Float4& a, const Float4& b) noexcept
{
  a = a + b;
  return a;
}

constexpr Float4& operator-=(Float4& a, const Float4& b) noexcept
{
  a = a - b;
  return a;
}

constexpr Float4& operator*=(Float4& a, const Float4& b) noexcept
{
  a = a * b;
  return a;
}

constexpr Float4& operator*=(Float4& v, f32 s) noexcept
{
  v = v * s;
  return v;
}

constexpr Float4& operator/=(Float4& a, const Float4& b) noexcept
{
  a = a / b;
  return a;
}

constexpr Float4& operator/=(Float4& v, f32 s) noexcept
{
  v = v / s;
  return v;
}

[[nodiscard]] constexpr f32 Dot(const Float4& a, const Float4& b) noexcept
{
  return a.X * b.X + a.Y * b.Y + a.Z * b.Z + a.W * b.W;
}

[[nodiscard]] constexpr f32 LengthSquared(const Float4& v) noexcept
{
  return v.X * v.X + v.Y * v.Y + v.Z * v.Z + v.W * v.W;
}

[[nodiscard]] constexpr f32 DistanceSquared(const Float4& a, const Float4& b) noexcept
{
  return LengthSquared(a - b);
}

[[nodiscard]] inline f32 Length(const Float4& v) noexcept
{
  return ::std::sqrt(LengthSquared(v));
}

[[nodiscard]] inline f32 Distance(const Float4& a, const Float4& b) noexcept
{
  return Length(a - b);
}

[[nodiscard]] inline Float4 Normalized(const Float4& v) noexcept
{
  const f32 len = Length(v);
  return len > 0.0f ? (v / len) : Float4 {};
}

[[nodiscard]] constexpr Float4 Min(const Float4& a, const Float4& b) noexcept
{
  return {Min(a.X, b.X), Min(a.Y, b.Y), Min(a.Z, b.Z), Min(a.W, b.W)};
}

[[nodiscard]] constexpr Float4 Max(const Float4& a, const Float4& b) noexcept
{
  return {Max(a.X, b.X), Max(a.Y, b.Y), Max(a.Z, b.Z), Max(a.W, b.W)};
}

[[nodiscard]] constexpr Float4 Clamp(const Float4& v, const Float4& min, const Float4& max) noexcept
{
  return {Clamp(v.X, min.X, max.X), Clamp(v.Y, min.Y, max.Y), Clamp(v.Z, min.Z, max.Z), Clamp(v.W, min.W, max.W)};
}

[[nodiscard]] constexpr Float4 Lerp(const Float4& a, const Float4& b, f32 t) noexcept
{
  return {Lerp(a.X, b.X, t), Lerp(a.Y, b.Y, t), Lerp(a.Z, b.Z, t), Lerp(a.W, b.W, t)};
}

[[nodiscard]] constexpr Float4 Abs(const Float4& v) noexcept
{
  return {Abs(v.X), Abs(v.Y), Abs(v.Z), Abs(v.W)};
}

[[nodiscard]] constexpr Float4 Reflect(const Float4& incident, const Float4& normal) noexcept
{
  return incident - normal * (2.0f * Dot(incident, normal));
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

constexpr Int2 operator-(const Int2& v) noexcept
{
  return {-v.X, -v.Y};
}

constexpr Int2 operator*(const Int2& a, const Int2& b) noexcept
{
  return {a.X * b.X, a.Y * b.Y};
}

constexpr Int2 operator*(const Int2& v, i32 s) noexcept
{
  return {v.X * s, v.Y * s};
}

constexpr Int2 operator*(i32 s, const Int2& v) noexcept
{
  return {v.X * s, v.Y * s};
}

constexpr Int2 operator/(const Int2& v, i32 s) noexcept
{
  return {v.X / s, v.Y / s};
}

constexpr Int2 operator/(const Int2& a, const Int2& b) noexcept
{
  return {a.X / b.X, a.Y / b.Y};
}

constexpr Int2& operator+=(Int2& a, const Int2& b) noexcept
{
  a = a + b;
  return a;
}

constexpr Int2& operator-=(Int2& a, const Int2& b) noexcept
{
  a = a - b;
  return a;
}

constexpr Int2& operator*=(Int2& a, const Int2& b) noexcept
{
  a = a * b;
  return a;
}

constexpr Int2& operator*=(Int2& v, i32 s) noexcept
{
  v = v * s;
  return v;
}

constexpr Int2& operator/=(Int2& a, const Int2& b) noexcept
{
  a = a / b;
  return a;
}

constexpr Int2& operator/=(Int2& v, i32 s) noexcept
{
  v = v / s;
  return v;
}

[[nodiscard]] constexpr i32 Dot(const Int2& a, const Int2& b) noexcept
{
  return a.X * b.X + a.Y * b.Y;
}

[[nodiscard]] constexpr Int2 Min(const Int2& a, const Int2& b) noexcept
{
  return {a.X < b.X ? a.X : b.X, a.Y < b.Y ? a.Y : b.Y};
}

[[nodiscard]] constexpr Int2 Max(const Int2& a, const Int2& b) noexcept
{
  return {a.X > b.X ? a.X : b.X, a.Y > b.Y ? a.Y : b.Y};
}

[[nodiscard]] constexpr Int2 Clamp(const Int2& v, const Int2& min, const Int2& max) noexcept
{
  return {v.X < min.X ? min.X : (v.X > max.X ? max.X : v.X), v.Y < min.Y ? min.Y : (v.Y > max.Y ? max.Y : v.Y)};
}

[[nodiscard]] constexpr Int2 Abs(const Int2& v) noexcept
{
  return {Abs(v.X), Abs(v.Y)};
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

constexpr Int3 operator-(const Int3& v) noexcept
{
  return {-v.X, -v.Y, -v.Z};
}

constexpr Int3 operator*(const Int3& a, const Int3& b) noexcept
{
  return {a.X * b.X, a.Y * b.Y, a.Z * b.Z};
}

constexpr Int3 operator*(const Int3& v, i32 s) noexcept
{
  return {v.X * s, v.Y * s, v.Z * s};
}

constexpr Int3 operator*(i32 s, const Int3& v) noexcept
{
  return {v.X * s, v.Y * s, v.Z * s};
}

constexpr Int3 operator/(const Int3& v, i32 s) noexcept
{
  return {v.X / s, v.Y / s, v.Z / s};
}

constexpr Int3 operator/(const Int3& a, const Int3& b) noexcept
{
  return {a.X / b.X, a.Y / b.Y, a.Z / b.Z};
}

constexpr Int3& operator+=(Int3& a, const Int3& b) noexcept
{
  a = a + b;
  return a;
}

constexpr Int3& operator-=(Int3& a, const Int3& b) noexcept
{
  a = a - b;
  return a;
}

constexpr Int3& operator*=(Int3& a, const Int3& b) noexcept
{
  a = a * b;
  return a;
}

constexpr Int3& operator*=(Int3& v, i32 s) noexcept
{
  v = v * s;
  return v;
}

constexpr Int3& operator/=(Int3& a, const Int3& b) noexcept
{
  a = a / b;
  return a;
}

constexpr Int3& operator/=(Int3& v, i32 s) noexcept
{
  v = v / s;
  return v;
}

[[nodiscard]] constexpr i32 Dot(const Int3& a, const Int3& b) noexcept
{
  return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
}

[[nodiscard]] constexpr Int3 Min(const Int3& a, const Int3& b) noexcept
{
  return {a.X < b.X ? a.X : b.X, a.Y < b.Y ? a.Y : b.Y, a.Z < b.Z ? a.Z : b.Z};
}

[[nodiscard]] constexpr Int3 Max(const Int3& a, const Int3& b) noexcept
{
  return {a.X > b.X ? a.X : b.X, a.Y > b.Y ? a.Y : b.Y, a.Z > b.Z ? a.Z : b.Z};
}

[[nodiscard]] constexpr Int3 Clamp(const Int3& v, const Int3& min, const Int3& max) noexcept
{
  return {v.X < min.X ? min.X : (v.X > max.X ? max.X : v.X), v.Y < min.Y ? min.Y : (v.Y > max.Y ? max.Y : v.Y),
          v.Z < min.Z ? min.Z : (v.Z > max.Z ? max.Z : v.Z)};
}

[[nodiscard]] constexpr Int3 Abs(const Int3& v) noexcept
{
  return {Abs(v.X), Abs(v.Y), Abs(v.Z)};
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

constexpr Int4 operator-(const Int4& v) noexcept
{
  return {-v.X, -v.Y, -v.Z, -v.W};
}

constexpr Int4 operator*(const Int4& a, const Int4& b) noexcept
{
  return {a.X * b.X, a.Y * b.Y, a.Z * b.Z, a.W * b.W};
}

constexpr Int4 operator*(const Int4& v, i32 s) noexcept
{
  return {v.X * s, v.Y * s, v.Z * s, v.W * s};
}

constexpr Int4 operator*(i32 s, const Int4& v) noexcept
{
  return {v.X * s, v.Y * s, v.Z * s, v.W * s};
}

constexpr Int4 operator/(const Int4& v, i32 s) noexcept
{
  return {v.X / s, v.Y / s, v.Z / s, v.W / s};
}

constexpr Int4 operator/(const Int4& a, const Int4& b) noexcept
{
  return {a.X / b.X, a.Y / b.Y, a.Z / b.Z, a.W / b.W};
}

constexpr Int4& operator+=(Int4& a, const Int4& b) noexcept
{
  a = a + b;
  return a;
}

constexpr Int4& operator-=(Int4& a, const Int4& b) noexcept
{
  a = a - b;
  return a;
}

constexpr Int4& operator*=(Int4& a, const Int4& b) noexcept
{
  a = a * b;
  return a;
}

constexpr Int4& operator*=(Int4& v, i32 s) noexcept
{
  v = v * s;
  return v;
}

constexpr Int4& operator/=(Int4& a, const Int4& b) noexcept
{
  a = a / b;
  return a;
}

constexpr Int4& operator/=(Int4& v, i32 s) noexcept
{
  v = v / s;
  return v;
}

[[nodiscard]] constexpr i32 Dot(const Int4& a, const Int4& b) noexcept
{
  return a.X * b.X + a.Y * b.Y + a.Z * b.Z + a.W * b.W;
}

[[nodiscard]] constexpr Int4 Min(const Int4& a, const Int4& b) noexcept
{
  return {a.X < b.X ? a.X : b.X, a.Y < b.Y ? a.Y : b.Y, a.Z < b.Z ? a.Z : b.Z, a.W < b.W ? a.W : b.W};
}

[[nodiscard]] constexpr Int4 Max(const Int4& a, const Int4& b) noexcept
{
  return {a.X > b.X ? a.X : b.X, a.Y > b.Y ? a.Y : b.Y, a.Z > b.Z ? a.Z : b.Z, a.W > b.W ? a.W : b.W};
}

[[nodiscard]] constexpr Int4 Clamp(const Int4& v, const Int4& min, const Int4& max) noexcept
{
  return {v.X < min.X ? min.X : (v.X > max.X ? max.X : v.X), v.Y < min.Y ? min.Y : (v.Y > max.Y ? max.Y : v.Y),
          v.Z < min.Z ? min.Z : (v.Z > max.Z ? max.Z : v.Z), v.W < min.W ? min.W : (v.W > max.W ? max.W : v.W)};
}

[[nodiscard]] constexpr Int4 Abs(const Int4& v) noexcept
{
  return {Abs(v.X), Abs(v.Y), Abs(v.Z), Abs(v.W)};
}

// Convenience aliases
/// Lowercase alias for `Float2` (HLSL/GLSL spelling).
using float2 = Float2;
/// Lowercase alias for `Float3` (HLSL/GLSL spelling).
using float3 = Float3;
/// Lowercase alias for `Float4` (HLSL/GLSL spelling).
using float4 = Float4;
/// Lowercase alias for `Int2`.
using int2 = Int2;
/// Lowercase alias for `Int3`.
using int3 = Int3;
/// Lowercase alias for `Int4`.
using int4 = Int4;

/// Semantic alias: a 2D point in space.
using Point2 = Float2;
/// Semantic alias: a 3D point in space.
using Point3 = Float3;
/// Semantic alias: a 4D (homogeneous) point in space.
using Point4 = Float4;
/// Semantic alias: a 2D integer point.
using Point2i = Int2;
/// Semantic alias: a 3D integer point.
using Point3i = Int3;
/// Semantic alias: a 4D integer point.
using Point4i = Int4;

/// Semantic alias: a 2D size (width, height) as floats.
using Size2 = Float2;
/// Semantic alias: a 3D size as floats.
using Size3 = Float3;
/// Semantic alias: a 4D size as floats.
using Size4 = Float4;
/// Semantic alias: a 2D size (width, height) as integers.
using Size2i = Int2;
/// Semantic alias: a 3D integer size.
using Size3i = Int3;
/// Semantic alias: a 4D integer size.
using Size4i = Int4;

}  // namespace gecko::math
