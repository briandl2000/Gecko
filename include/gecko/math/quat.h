#pragma once

/// @file
/// Quaternion (and basic API alias `Rotor` from `matrix.h`) for compact
/// 3D rotation representation.
///
/// `Quat` stores `(X, Y, Z, W)` where `W` is the scalar (real) part and
/// `(X, Y, Z)` are the imaginary axis-scaled-by-`sin(theta/2)`
/// components. Unit quaternions represent rotations.
///
/// Conventions:
/// - Composition: `q2 * q1` applies `q1` first, then `q2`.
/// - Rotate a vector via `Rotate(q, v)`; equivalent to `q * v * q^-1`.
/// - `Inverse(q) == Conjugate(q)` when `q` is unit-length.
/// - All angles are in radians.

#include "gecko/math/vector.h"

namespace gecko::math {

// Forward declarations
struct Float3x3;
struct Float4x4;

/// Quaternion: a compact 4-float rotation representation
/// (`W + Xi + Yj + Zk`). Default constructs to identity (`0,0,0,1`).
/// Compared with Euler angles, quaternions avoid gimbal lock and
/// interpolate smoothly via `Slerp`.
struct Quat
{
  union
  {
    struct
    {
      f32 X;  ///< i (imaginary) component.
      f32 Y;  ///< j (imaginary) component.
      f32 Z;  ///< k (imaginary) component.
      f32 W;  ///< Scalar (real) component.
    };
    f32 Data[4];  ///< Raw component storage.
  };

  /// Default constructor: identity quaternion `(0,0,0,1)`.
  constexpr Quat() noexcept : X(0.0f), Y(0.0f), Z(0.0f), W(1.0f)
  {}

  /// Construct from raw components.
  constexpr Quat(f32 x, f32 y, f32 z, f32 w) noexcept : X(x), Y(y), Z(z), W(w)
  {}

  /// Build a rotation of `angle` radians around `axis`.
  /// `axis` is **not** re-normalized; use `AxisAngle` if it might not be unit.
  Quat(const Float3& axis, f32 angle) noexcept
      : X(axis.X * Sin(angle * 0.5f)), Y(axis.Y * Sin(angle * 0.5f)), Z(axis.Z * Sin(angle * 0.5f)),
        W(Cos(angle * 0.5f))
  {}

  [[nodiscard]] constexpr f32& operator[](usize index) noexcept
  {
    return Data[index];
  }

  [[nodiscard]] constexpr const f32& operator[](usize index) const noexcept
  {
    return Data[index];
  }

  /// Returns the identity rotation `(0,0,0,1)`.
  static constexpr Quat Identity() noexcept
  {
    return {};
  }

  /// Build a rotation of `angle` radians around `axis`. `axis` is
  /// normalized internally so any length is acceptable (non-zero).
  static inline Quat AxisAngle(const Float3& axis, f32 angle) noexcept
  {
    const Float3 normalized = Normalized(axis);
    const f32 halfAngle = angle * 0.5f;
    const f32 s = Sin(halfAngle);
    return {normalized.X * s, normalized.Y * s, normalized.Z * s, Cos(halfAngle)};
  }

  /// Build the shortest-arc rotation that takes `from` to `to`.
  /// Inputs need not be unit length; both are normalized internally.
  /// Falls back to a 180-degree rotation around an arbitrary perpendicular
  /// axis when the inputs are antiparallel.
  static inline Quat FromTo(const Float3& from, const Float3& to) noexcept
  {
    const Float3 f = Normalized(from);
    const Float3 t = Normalized(to);
    const f32 dot = Dot(f, t);

    // Vectors are parallel
    if (dot >= 1.0f - Epsilon)
    {
      return Identity();
    }

    // Vectors are opposite
    if (dot <= -1.0f + Epsilon)
    {
      // Find perpendicular axis
      Float3 perp = Cross({1.0f, 0.0f, 0.0f}, f);
      if (LengthSquared(perp) < Epsilon)
      {
        perp = Cross({0.0f, 1.0f, 0.0f}, f);
      }
      return AxisAngle(Normalized(perp), Pi);
    }

    // General case
    const Float3 cross = Cross(f, t);
    const f32 s = Sqrt((1.0f + dot) * 2.0f);
    const f32 invS = 1.0f / s;

    return {cross.X * invS, cross.Y * invS, cross.Z * invS, s * 0.5f};
  }

  /// Build a rotation from intrinsic Tait-Bryan Euler angles in radians.
  /// @param pitch  Rotation around the X axis (pitch).
  /// @param yaw    Rotation around the Y axis (yaw).
  /// @param roll   Rotation around the Z axis (roll).
  static inline Quat Euler(f32 pitch, f32 yaw, f32 roll) noexcept
  {
    const f32 cy = Cos(yaw * 0.5f);
    const f32 sy = Sin(yaw * 0.5f);
    const f32 cp = Cos(pitch * 0.5f);
    const f32 sp = Sin(pitch * 0.5f);
    const f32 cr = Cos(roll * 0.5f);
    const f32 sr = Sin(roll * 0.5f);

    return {sr * cp * cy - cr * sp * sy, cr * sp * cy + sr * cp * sy, cr * cp * sy - sr * sp * cy,
            cr * cp * cy + sr * sp * sy};
  }

  // TODO: Implement when matrix conversions are needed
  // static Quat FromMatrix(const Float3x3& m) noexcept;
  // static Quat FromMatrix(const Float4x4& m) noexcept;
};

// Quaternion operations
[[nodiscard]] constexpr bool operator==(const Quat& a, const Quat& b) noexcept
{
  return a.X == b.X && a.Y == b.Y && a.Z == b.Z && a.W == b.W;
}

[[nodiscard]] constexpr bool operator!=(const Quat& a, const Quat& b) noexcept
{
  return !(a == b);
}

/// Hamilton product. Composing rotations: `q2 * q1` applies `q1` first.
[[nodiscard]] constexpr Quat operator*(const Quat& a, const Quat& b) noexcept
{
  return {a.W * b.X + a.X * b.W + a.Y * b.Z - a.Z * b.Y, a.W * b.Y - a.X * b.Z + a.Y * b.W + a.Z * b.X,
          a.W * b.Z + a.X * b.Y - a.Y * b.X + a.Z * b.W, a.W * b.W - a.X * b.X - a.Y * b.Y - a.Z * b.Z};
}

// Scalar multiplication
[[nodiscard]] constexpr Quat operator*(const Quat& q, f32 s) noexcept
{
  return {q.X * s, q.Y * s, q.Z * s, q.W * s};
}

[[nodiscard]] constexpr Quat operator*(f32 s, const Quat& q) noexcept
{
  return {q.X * s, q.Y * s, q.Z * s, q.W * s};
}

// Addition
[[nodiscard]] constexpr Quat operator+(const Quat& a, const Quat& b) noexcept
{
  return {a.X + b.X, a.Y + b.Y, a.Z + b.Z, a.W + b.W};
}

/// Quaternion dot product (treats `Quat` as a 4-vector).
[[nodiscard]] constexpr f32 Dot(const Quat& a, const Quat& b) noexcept
{
  return a.X * b.X + a.Y * b.Y + a.Z * b.Z + a.W * b.W;
}

/// Squared magnitude `|q|^2 = X^2+Y^2+Z^2+W^2`.
[[nodiscard]] constexpr f32 LengthSquared(const Quat& q) noexcept
{
  return q.X * q.X + q.Y * q.Y + q.Z * q.Z + q.W * q.W;
}

/// Magnitude `|q|`. Unit quaternions have `Length(q) == 1`.
[[nodiscard]] inline f32 Length(const Quat& q) noexcept
{
  return Sqrt(LengthSquared(q));
}

/// Returns `q` scaled to unit length, or `Identity()` when `|q| ~ 0`.
[[nodiscard]] inline Quat Normalized(const Quat& q) noexcept
{
  const f32 len = Length(q);
  return len > Epsilon ? (q * (1.0f / len)) : Quat::Identity();
}

/// Conjugate `(-X, -Y, -Z, W)`. For unit quaternions this is the inverse
/// rotation.
[[nodiscard]] constexpr Quat Conjugate(const Quat& q) noexcept
{
  return {-q.X, -q.Y, -q.Z, q.W};
}

/// Multiplicative inverse. Equals `Conjugate(q)` when `q` is unit-length.
[[nodiscard]] inline Quat Inverse(const Quat& q) noexcept
{
  const f32 lenSq = LengthSquared(q);
  return lenSq > Epsilon ? (Conjugate(q) * (1.0f / lenSq)) : Quat::Identity();
}

/// Rotate a 3D vector by a unit quaternion. Equivalent to `q * v * q^-1`.
[[nodiscard]] inline Float3 Rotate(const Quat& q, const Float3& v) noexcept
{
  // Optimized formula: v' = v + 2w(q_xyz x v) + 2(q_xyz x (q_xyz x v))
  const Float3 qv {q.X, q.Y, q.Z};
  const Float3 cross1 = Cross(qv, v);
  const Float3 cross2 = Cross(qv, cross1);
  return v + ((cross1 * q.W) + cross2) * 2.0f;
}

/// Spherical linear interpolation between unit quaternions `a` and `b`.
/// Picks the shorter arc by negating `b` when `Dot(a, b) < 0`.
/// `t` in `[0, 1]`.
[[nodiscard]] inline Quat Slerp(const Quat& a, const Quat& b, f32 t) noexcept
{
  Quat q1 = a;
  Quat q2 = b;

  f32 dot = Dot(q1, q2);

  // If dot < 0, negate one quaternion to take shorter path
  if (dot < 0.0f)
  {
    q2 = q2 * -1.0f;
    dot = -dot;
  }

  // If quaternions are very close, use linear interpolation
  if (dot > 1.0f - Epsilon)
  {
    return Normalized(q1 + (q2 + q1 * -1.0f) * t);
  }

  const f32 theta = ::std::acos(dot);
  const f32 sinTheta = Sin(theta);
  const f32 w1 = Sin((1.0f - t) * theta) / sinTheta;
  const f32 w2 = Sin(t * theta) / sinTheta;

  return q1 * w1 + q2 * w2;
}

/// Normalized linear interpolation: faster than `Slerp` but with non-uniform
/// angular speed. Acceptable for tween-like blends. `t` in `[0, 1]`.
[[nodiscard]] inline Quat Nlerp(const Quat& a, const Quat& b, f32 t) noexcept
{
  Quat q2 = b;

  // Take shorter path
  if (Dot(a, b) < 0.0f)
  {
    q2 = q2 * -1.0f;
  }

  return Normalized(a + (q2 + a * -1.0f) * t);
}

/// Decompose a unit quaternion into its axis/angle representation.
/// @param q         Unit quaternion (caller's responsibility, but normalized
/// internally).
/// @param outAxis   Receives the rotation axis (or `(1,0,0)` if angle ~ 0).
/// @param outAngle  Receives the angle in radians, in `[0, 2*Pi]`.
inline void ToAxisAngle(const Quat& q, Float3& outAxis, f32& outAngle) noexcept
{
  const Quat normalized = Normalized(q);
  outAngle = 2.0f * ::std::acos(normalized.W);
  const f32 s = Sqrt(1.0f - normalized.W * normalized.W);

  if (s < Epsilon)
  {
    outAxis = {1.0f, 0.0f, 0.0f};
  }
  else
  {
    outAxis = {normalized.X / s, normalized.Y / s, normalized.Z / s};
  }
}

// TODO: Implement when matrix conversions are needed
// Float3x3 ToMatrix3(const Quat& q) noexcept;
// Float4x4 ToMatrix4(const Quat& q) noexcept;

}  // namespace gecko::math
