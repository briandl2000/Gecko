#pragma once

#include "gecko/math/vector.h"

namespace gecko::math {

// Forward declarations
struct Float3x3;
struct Float4x4;

// Quaternion - compact rotation representation
// =============================================
// Represents rotations in 3D space using 4 components (w, x, y, z)
//
// Format: W + Xi + Yj + Zk
//   - W: scalar (real) component
//   - X, Y, Z: imaginary components representing rotation axis scaled by
//   sin(θ/2)
//
// Key concepts:
//   - Unit quaternions represent rotations (|q| = 1)
//   - Apply rotation via: v' = q * v * q^(-1) (conjugate for unit quats)
//   - Compose rotations: q_combined = q2 * q1 (applies q1 first, then q2)
//   - Inverse rotation: q* (conjugate - negate imaginary parts)
//
// Advantages:
//   - Compact (4 floats vs 9 for matrix)
//   - No gimbal lock (unlike Euler angles)
//   - Smooth interpolation via Slerp
//   - Easy to normalize and compose
//   - Industry standard for animation and physics
struct Quat
{
  union
  {
    struct
    {
      f32 X;  // i component
      f32 Y;  // j component
      f32 Z;  // k component
      f32 W;  // Scalar (real) component
    };
    f32 Data[4];
  };

  constexpr Quat() noexcept : X(0.0f), Y(0.0f), Z(0.0f), W(1.0f)
  {}

  constexpr Quat(f32 x, f32 y, f32 z, f32 w) noexcept : X(x), Y(y), Z(z), W(w)
  {}

  // Create from axis and angle
  Quat(const Float3& axis, f32 angle) noexcept
      : X(axis.X * Sin(angle * 0.5f)), Y(axis.Y * Sin(angle * 0.5f)),
        Z(axis.Z * Sin(angle * 0.5f)), W(Cos(angle * 0.5f))
  {}

  [[nodiscard]] constexpr f32& operator[](usize index) noexcept
  {
    return Data[index];
  }

  [[nodiscard]] constexpr const f32& operator[](usize index) const noexcept
  {
    return Data[index];
  }

  static constexpr Quat Identity() noexcept
  {
    return {};
  }

  // Create from axis-angle
  static inline Quat AxisAngle(const Float3& axis, f32 angle) noexcept
  {
    const Float3 normalized = Normalized(axis);
    const f32 halfAngle = angle * 0.5f;
    const f32 s = Sin(halfAngle);
    return {normalized.X * s, normalized.Y * s, normalized.Z * s,
            Cos(halfAngle)};
  }

  // Create quaternion from one vector to another
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

  // Euler angles (in radians): pitch (X), yaw (Y), roll (Z)
  static inline Quat Euler(f32 pitch, f32 yaw, f32 roll) noexcept
  {
    const f32 cy = Cos(yaw * 0.5f);
    const f32 sy = Sin(yaw * 0.5f);
    const f32 cp = Cos(pitch * 0.5f);
    const f32 sp = Sin(pitch * 0.5f);
    const f32 cr = Cos(roll * 0.5f);
    const f32 sr = Sin(roll * 0.5f);

    return {sr * cp * cy - cr * sp * sy, cr * sp * cy + sr * cp * sy,
            cr * cp * sy - sr * sp * cy, cr * cp * cy + sr * sp * sy};
  }

  // Create from rotation matrix
  static inline Quat FromMatrix(const Float3x3& m) noexcept;
  static inline Quat FromMatrix(const Float4x4& m) noexcept;
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

// Quaternion multiplication (Hamilton product - composition of rotations)
// q = q2 * q1 applies q1 first, then q2
[[nodiscard]] constexpr Quat operator*(const Quat& a, const Quat& b) noexcept
{
  return {a.W * b.X + a.X * b.W + a.Y * b.Z - a.Z * b.Y,
          a.W * b.Y - a.X * b.Z + a.Y * b.W + a.Z * b.X,
          a.W * b.Z + a.X * b.Y - a.Y * b.X + a.Z * b.W,
          a.W * b.W - a.X * b.X - a.Y * b.Y - a.Z * b.Z};
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

// Dot product
[[nodiscard]] constexpr f32 Dot(const Quat& a, const Quat& b) noexcept
{
  return a.X * b.X + a.Y * b.Y + a.Z * b.Z + a.W * b.W;
}

// Magnitude squared
[[nodiscard]] constexpr f32 LengthSquared(const Quat& q) noexcept
{
  return q.X * q.X + q.Y * q.Y + q.Z * q.Z + q.W * q.W;
}

// Magnitude
[[nodiscard]] inline f32 Length(const Quat& q) noexcept
{
  return Sqrt(LengthSquared(q));
}

// Normalize
[[nodiscard]] inline Quat Normalized(const Quat& q) noexcept
{
  const f32 len = Length(q);
  return len > Epsilon ? (q * (1.0f / len)) : Quat::Identity();
}

// Conjugate (inverse rotation for unit quaternions)
// q* negates the imaginary parts
[[nodiscard]] constexpr Quat Conjugate(const Quat& q) noexcept
{
  return {-q.X, -q.Y, -q.Z, q.W};
}

// Inverse
[[nodiscard]] inline Quat Inverse(const Quat& q) noexcept
{
  const f32 lenSq = LengthSquared(q);
  return lenSq > Epsilon ? (Conjugate(q) * (1.0f / lenSq)) : Quat::Identity();
}

// Rotate a vector by a quaternion: v' = q * v * q^(-1)
[[nodiscard]] inline Float3 Rotate(const Quat& q, const Float3& v) noexcept
{
  // Optimized formula: v' = v + 2w(q_xyz × v) + 2(q_xyz × (q_xyz × v))
  const Float3 qv {q.X, q.Y, q.Z};
  const Float3 cross1 = Cross(qv, v);
  const Float3 cross2 = Cross(qv, cross1);
  return v + ((cross1 * q.W) + cross2) * 2.0f;
}

// Spherical linear interpolation
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

// Normalized linear interpolation (faster approximation)
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

// Extract axis and angle from quaternion
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

// Convert to 3x3 rotation matrix
inline Float3x3 ToMatrix3(const Quat& q) noexcept;

// Convert to 4x4 rotation matrix
inline Float4x4 ToMatrix4(const Quat& q) noexcept;

}  // namespace gecko::math
