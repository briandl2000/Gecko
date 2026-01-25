#pragma once

#include "gecko/math/vector.h"

namespace gecko::math {

// Forward declarations
struct Float3x3;
struct Float4x4;

// Rotor - geometric algebra rotation operator
// ============================================
// Represents rotations in 3D space as even-grade multivectors (scalar +
// bivector)
//
// Format: S + E23*e23 + E31*e31 + E12*e12
//   - S: scalar component
//   - E23, E31, E12: bivector components representing oriented planes
//     - E23 (e2∧e3): YZ plane rotation
//     - E31 (e3∧e1): ZX plane rotation
//     - E12 (e1∧e2): XY plane rotation
//
// Key concepts:
//   - Unlike quaternions which have a "vector" imaginary part,
//     rotors have a BIVECTOR (plane) part - more geometrically intuitive
//   - The bivector represents the PLANE of rotation, not the axis
//   - Apply rotation via sandwich product: v' = R * v * ~R
//   - Compose rotations: R_combined = R2 * R1 (applies R1 first, then R2)
//   - Inverse rotation: ~R (reverse operation - negate bivector parts)
//
// Advantages over quaternions:
//   - More geometric (plane vs axis thinking)
//   - Part of larger geometric algebra framework
//   - Extends naturally to higher dimensions
//   - Can be extended to motors (rotation + translation in PGA)
struct Rotor
{
  union
  {
    struct
    {
      f32 E23;  // YZ plane (bivector component)
      f32 E31;  // ZX plane (bivector component)
      f32 E12;  // XY plane (bivector component)
      f32 S;    // Scalar component
    };
    f32 Data[4];
  };

  constexpr Rotor() noexcept : E23(0.0f), E31(0.0f), E12(0.0f), S(1.0f)
  {}

  constexpr Rotor(f32 e23, f32 e31, f32 e12, f32 s) noexcept
      : E23(e23), E31(e31), E12(e12), S(s)
  {}

  // Create from plane (bivector) and angle
  // The bivector represents the plane of rotation (not axis!)
  constexpr Rotor(const Float3& bivector, f32 angle) noexcept
      : E23(bivector.X * Sin(angle * 0.5f)),
        E31(bivector.Y * Sin(angle * 0.5f)),
        E12(bivector.Z * Sin(angle * 0.5f)), S(Cos(angle * 0.5f))
  {}

  [[nodiscard]] constexpr f32& operator[](usize index) noexcept
  {
    return Data[index];
  }

  [[nodiscard]] constexpr const f32& operator[](usize index) const noexcept
  {
    return Data[index];
  }

  static constexpr Rotor Identity() noexcept
  {
    return {};
  }

  // Create from axis-angle
  // Note: Internally converts axis to dual (perpendicular plane)
  static inline Rotor AxisAngle(const Float3& axis, f32 angle) noexcept
  {
    const Float3 normalized = Normalized(axis);
    const f32 halfAngle = angle * 0.5f;
    const f32 s = Sin(halfAngle);
    // Axis to bivector via Hodge dual (perpendicular plane)
    return {normalized.X * s, normalized.Y * s, normalized.Z * s,
            Cos(halfAngle)};
  }

  // Create rotor from one vector to another
  static inline Rotor FromTo(const Float3& from, const Float3& to) noexcept
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
      // Find perpendicular plane via cross product
      Float3 perp = Cross({1.0f, 0.0f, 0.0f}, f);
      if (LengthSquared(perp) < Epsilon)
      {
        perp = Cross({0.0f, 1.0f, 0.0f}, f);
      }
      return AxisAngle(Normalized(perp), Pi);
    }

    // The bivector is the wedge product (outer product) of the vectors
    const Float3 bivec = Cross(f, t);  // In 3D, wedge product = cross product
    const f32 s = Sqrt((1.0f + dot) * 2.0f);
    const f32 invS = 1.0f / s;

    return {bivec.X * invS, bivec.Y * invS, bivec.Z * invS, s * 0.5f};
  }

  // Euler angles (in radians): pitch (X), yaw (Y), roll (Z)
  static inline Rotor Euler(f32 pitch, f32 yaw, f32 roll) noexcept
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
  static inline Rotor FromMatrix(const Float3x3& m) noexcept;
  static inline Rotor FromMatrix(const Float4x4& m) noexcept;
};

// Rotor operations
[[nodiscard]] constexpr bool operator==(const Rotor& a, const Rotor& b) noexcept
{
  return a.E23 == b.E23 && a.E31 == b.E31 && a.E12 == b.E12 && a.S == b.S;
}

[[nodiscard]] constexpr bool operator!=(const Rotor& a, const Rotor& b) noexcept
{
  return !(a == b);
}

// Rotor multiplication (geometric product - composition of rotations)
// R = R2 * R1 applies R1 first, then R2
[[nodiscard]] constexpr Rotor operator*(const Rotor& a, const Rotor& b) noexcept
{
  // Geometric product of two even-grade multivectors
  return {a.S * b.E23 + a.E23 * b.S + a.E31 * b.E12 - a.E12 * b.E31,
          a.S * b.E31 - a.E23 * b.E12 + a.E31 * b.S + a.E12 * b.E23,
          a.S * b.E12 + a.E23 * b.E31 - a.E31 * b.E23 + a.E12 * b.S,
          a.S * b.S - a.E23 * b.E23 - a.E31 * b.E31 - a.E12 * b.E12};
}

// Scalar multiplication
[[nodiscard]] constexpr Rotor operator*(const Rotor& r, f32 s) noexcept
{
  return {r.E23 * s, r.E31 * s, r.E12 * s, r.S * s};
}

[[nodiscard]] constexpr Rotor operator*(f32 s, const Rotor& r) noexcept
{
  return {r.E23 * s, r.E31 * s, r.E12 * s, r.S * s};
}

// Addition
[[nodiscard]] constexpr Rotor operator+(const Rotor& a, const Rotor& b) noexcept
{
  return {a.E23 + b.E23, a.E31 + b.E31, a.E12 + b.E12, a.S + b.S};
}

// Dot product (scalar part of geometric product)
[[nodiscard]] constexpr f32 Dot(const Rotor& a, const Rotor& b) noexcept
{
  return a.E23 * b.E23 + a.E31 * b.E31 + a.E12 * b.E12 + a.S * b.S;
}

// Magnitude squared
[[nodiscard]] constexpr f32 LengthSquared(const Rotor& r) noexcept
{
  return r.E23 * r.E23 + r.E31 * r.E31 + r.E12 * r.E12 + r.S * r.S;
}

// Magnitude
[[nodiscard]] inline f32 Length(const Rotor& r) noexcept
{
  return Sqrt(LengthSquared(r));
}

// Normalize
[[nodiscard]] inline Rotor Normalized(const Rotor& r) noexcept
{
  const f32 len = Length(r);
  return len > Epsilon ? (r * (1.0f / len)) : Rotor::Identity();
}

// Reverse (reverse of geometric product - inverse rotation for unit rotors)
// ~R inverts the bivector signs
[[nodiscard]] constexpr Rotor Reverse(const Rotor& r) noexcept
{
  return {-r.E23, -r.E31, -r.E12, r.S};
}

// Inverse
[[nodiscard]] inline Rotor Inverse(const Rotor& r) noexcept
{
  const f32 lenSq = LengthSquared(r);
  return lenSq > Epsilon ? (Reverse(r) * (1.0f / lenSq)) : Rotor::Identity();
}

// Rotate a vector by a rotor using sandwich product: v' = R * v * ~R
// This is the fundamental rotation operation in geometric algebra
[[nodiscard]] inline Float3 Rotate(const Rotor& r, const Float3& v) noexcept
{
  // Optimized sandwich product: R * v * ~R
  const Float3 bivec {r.E23, r.E31, r.E12};
  const Float3 cross1 = Cross(bivec, v);
  const Float3 cross2 = Cross(bivec, cross1);
  return v + ((cross1 * r.S) + cross2) * 2.0f;
}

// Spherical linear interpolation
[[nodiscard]] inline Rotor Slerp(const Rotor& a, const Rotor& b, f32 t) noexcept
{
  Rotor r1 = a;
  Rotor r2 = b;

  f32 dot = Dot(r1, r2);

  // If dot < 0, negate one rotor to take shorter path
  if (dot < 0.0f)
  {
    r2 = r2 * -1.0f;
    dot = -dot;
  }

  // If rotors are very close, use linear interpolation
  if (dot > 1.0f - Epsilon)
  {
    return Normalized(r1 + (r2 + r1 * -1.0f) * t);
  }

  const f32 theta = ::std::acos(dot);
  const f32 sinTheta = Sin(theta);
  const f32 w1 = Sin((1.0f - t) * theta) / sinTheta;
  const f32 w2 = Sin(t * theta) / sinTheta;

  return r1 * w1 + r2 * w2;
}

// Normalized linear interpolation (faster approximation)
[[nodiscard]] inline Rotor Nlerp(const Rotor& a, const Rotor& b, f32 t) noexcept
{
  Rotor r2 = b;

  // Take shorter path
  if (Dot(a, b) < 0.0f)
  {
    r2 = r2 * -1.0f;
  }

  return Normalized(a + (r2 + a * -1.0f) * t);
}

// Extract plane (bivector) and angle from rotor
inline void ToPlanAngle(const Rotor& r, Float3& outBivector,
                        f32& outAngle) noexcept
{
  const Rotor normalized = Normalized(r);
  outAngle = 2.0f * ::std::acos(normalized.S);
  const f32 s = Sqrt(1.0f - normalized.S * normalized.S);

  if (s < Epsilon)
  {
    outBivector = {1.0f, 0.0f, 0.0f};
  }
  else
  {
    outBivector = {normalized.E23 / s, normalized.E31 / s, normalized.E12 / s};
  }
}

// Convert to 3x3 rotation matrix
inline Float3x3 ToMatrix3(const Rotor& r) noexcept;

// Convert to 4x4 rotation matrix
inline Float4x4 ToMatrix4(const Rotor& r) noexcept;

using quat = Rotor;  // Alias for compatibility

}  // namespace gecko::math
