#pragma once

#include "gecko/math/vector.h"

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4201)  // nonstandard extension: nameless struct/union
#endif

namespace gecko::math {

// Forward declaration
struct Rotor;

struct Float2x2
{
  union
  {
    struct
    {
      f32 M00;
      f32 M01;
      f32 M10;
      f32 M11;
    };
    f32 Data[4];
    Float2 Rows[2];
  };

  constexpr Float2x2() noexcept : M00(1.0f), M01(0.0f), M10(0.0f), M11(1.0f)
  {}

  constexpr Float2x2(f32 m00, f32 m01, f32 m10, f32 m11) noexcept
      : M00(m00), M01(m01), M10(m10), M11(m11)
  {}

  [[nodiscard]] constexpr Float2& operator[](usize index) noexcept
  {
    return Rows[index];
  }

  [[nodiscard]] constexpr const Float2& operator[](usize index) const noexcept
  {
    return Rows[index];
  }

  static constexpr Float2x2 Identity() noexcept
  {
    return {};
  }

  static inline Float2x2 Rotation(f32 angle) noexcept
  {
    const f32 c = ::gecko::math::Cos(angle);
    const f32 s = ::gecko::math::Sin(angle);
    return {c, -s, s, c};
  }
};

struct Float3x3
{
  union
  {
    struct
    {
      f32 M00;
      f32 M01;
      f32 M02;
      f32 M10;
      f32 M11;
      f32 M12;
      f32 M20;
      f32 M21;
      f32 M22;
    };
    f32 Data[9];
    Float3 Rows[3];
  };

  constexpr Float3x3() noexcept
      : M00(1.0f), M01(0.0f), M02(0.0f), M10(0.0f), M11(1.0f), M12(0.0f),
        M20(0.0f), M21(0.0f), M22(1.0f)
  {}

  constexpr Float3x3(f32 m00, f32 m01, f32 m02, f32 m10, f32 m11, f32 m12,
                     f32 m20, f32 m21, f32 m22) noexcept
      : M00(m00), M01(m01), M02(m02), M10(m10), M11(m11), M12(m12), M20(m20),
        M21(m21), M22(m22)
  {}

  [[nodiscard]] constexpr Float3& operator[](usize index) noexcept
  {
    return Rows[index];
  }

  [[nodiscard]] constexpr const Float3& operator[](usize index) const noexcept
  {
    return Rows[index];
  }

  static constexpr Float3x3 Identity() noexcept
  {
    return {};
  }

  static inline Float3x3 RotationX(f32 angle) noexcept
  {
    const f32 c = ::gecko::math::Cos(angle);
    const f32 s = ::gecko::math::Sin(angle);
    return {1.0f, 0.0f, 0.0f, 0.0f, c, -s, 0.0f, s, c};
  }

  static inline Float3x3 RotationY(f32 angle) noexcept
  {
    const f32 c = ::gecko::math::Cos(angle);
    const f32 s = ::gecko::math::Sin(angle);
    return {c, 0.0f, s, 0.0f, 1.0f, 0.0f, -s, 0.0f, c};
  }

  static inline Float3x3 RotationZ(f32 angle) noexcept
  {
    const f32 c = ::gecko::math::Cos(angle);
    const f32 s = ::gecko::math::Sin(angle);
    return {c, -s, 0.0f, s, c, 0.0f, 0.0f, 0.0f, 1.0f};
  }
};

struct Float4x4
{
  union
  {
    struct
    {
      f32 M00;
      f32 M01;
      f32 M02;
      f32 M03;
      f32 M10;
      f32 M11;
      f32 M12;
      f32 M13;
      f32 M20;
      f32 M21;
      f32 M22;
      f32 M23;
      f32 M30;
      f32 M31;
      f32 M32;
      f32 M33;
    };
    f32 Data[16];
    Float4 Rows[4];
  };

  constexpr Float4x4() noexcept
      : M00(1.0f), M01(0.0f), M02(0.0f), M03(0.0f), M10(0.0f), M11(1.0f),
        M12(0.0f), M13(0.0f), M20(0.0f), M21(0.0f), M22(1.0f), M23(0.0f),
        M30(0.0f), M31(0.0f), M32(0.0f), M33(1.0f)
  {}

  constexpr Float4x4(f32 m00, f32 m01, f32 m02, f32 m03, f32 m10, f32 m11,
                     f32 m12, f32 m13, f32 m20, f32 m21, f32 m22, f32 m23,
                     f32 m30, f32 m31, f32 m32, f32 m33) noexcept
      : M00(m00), M01(m01), M02(m02), M03(m03), M10(m10), M11(m11), M12(m12),
        M13(m13), M20(m20), M21(m21), M22(m22), M23(m23), M30(m30), M31(m31),
        M32(m32), M33(m33)
  {}

  [[nodiscard]] constexpr Float4& operator[](usize index) noexcept
  {
    return Rows[index];
  }

  [[nodiscard]] constexpr const Float4& operator[](usize index) const noexcept
  {
    return Rows[index];
  }

  static constexpr Float4x4 Identity() noexcept
  {
    return {};
  }

  static constexpr Float4x4 Translation(const Float3& t) noexcept
  {
    return {1.0f, 0.0f, 0.0f, t.X, 0.0f, 1.0f, 0.0f, t.Y,
            0.0f, 0.0f, 1.0f, t.Z, 0.0f, 0.0f, 0.0f, 1.0f};
  }

  static constexpr Float4x4 Scale(const Float3& s) noexcept
  {
    return {s.X,  0.0f, 0.0f, 0.0f, 0.0f, s.Y,  0.0f, 0.0f,
            0.0f, 0.0f, s.Z,  0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
  }

  static inline Float4x4 RotationX(f32 angle) noexcept
  {
    const f32 c = ::gecko::math::Cos(angle);
    const f32 s = ::gecko::math::Sin(angle);
    return {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, c,    -s,   0.0f,
            0.0f, s,    c,    0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
  }

  static inline Float4x4 RotationY(f32 angle) noexcept
  {
    const f32 c = ::gecko::math::Cos(angle);
    const f32 s = ::gecko::math::Sin(angle);
    return {c,  0.0f, s, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
            -s, 0.0f, c, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
  }

  static inline Float4x4 RotationZ(f32 angle) noexcept
  {
    const f32 c = ::gecko::math::Cos(angle);
    const f32 s = ::gecko::math::Sin(angle);
    return {c,    -s,   0.0f, 0.0f, s,    c,    0.0f, 0.0f,
            0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
  }

  static inline Float4x4 LookAt(const Float3& eye, const Float3& target,
                                const Float3& up) noexcept
  {
    const Float3 f = Normalized(target - eye);
    const Float3 r = Normalized(Cross(f, up));
    const Float3 u = Cross(r, f);
    return {r.X,  r.Y,  r.Z,  -Dot(r, eye), u.X,  u.Y,  u.Z,  -Dot(u, eye),
            -f.X, -f.Y, -f.Z, Dot(f, eye),  0.0f, 0.0f, 0.0f, 1.0f};
  }

  static inline Float4x4 Orthographic(f32 left, f32 right, f32 bottom, f32 top,
                                      f32 near, f32 far) noexcept
  {
    const f32 rl = 1.0f / (right - left);
    const f32 tb = 1.0f / (top - bottom);
    const f32 fn = 1.0f / (far - near);
    return {2.0f * rl, 0.0f,      0.0f,       -(right + left) * rl,
            0.0f,      2.0f * tb, 0.0f,       -(top + bottom) * tb,
            0.0f,      0.0f,      -2.0f * fn, -(far + near) * fn,
            0.0f,      0.0f,      0.0f,       1.0f};
  }

  static inline Float4x4 Perspective(f32 fovY, f32 aspect, f32 near,
                                     f32 far) noexcept
  {
    const f32 tanHalfFovy = ::gecko::math::Tan(fovY / 2.0f);
    const f32 fn = 1.0f / (far - near);
    return {1.0f / (aspect * tanHalfFovy),
            0.0f,
            0.0f,
            0.0f,
            0.0f,
            1.0f / tanHalfFovy,
            0.0f,
            0.0f,
            0.0f,
            0.0f,
            -(far + near) * fn,
            -2.0f * far * near * fn,
            0.0f,
            0.0f,
            -1.0f,
            0.0f};
  }
};

// Matrix multiply
constexpr Float2x2 operator*(const Float2x2& a, const Float2x2& b) noexcept
{
  return {a.M00 * b.M00 + a.M01 * b.M10, a.M00 * b.M01 + a.M01 * b.M11,
          a.M10 * b.M00 + a.M11 * b.M10, a.M10 * b.M01 + a.M11 * b.M11};
}

constexpr Float3x3 operator*(const Float3x3& a, const Float3x3& b) noexcept
{
  return {a.M00 * b.M00 + a.M01 * b.M10 + a.M02 * b.M20,
          a.M00 * b.M01 + a.M01 * b.M11 + a.M02 * b.M21,
          a.M00 * b.M02 + a.M01 * b.M12 + a.M02 * b.M22,
          a.M10 * b.M00 + a.M11 * b.M10 + a.M12 * b.M20,
          a.M10 * b.M01 + a.M11 * b.M11 + a.M12 * b.M21,
          a.M10 * b.M02 + a.M11 * b.M12 + a.M12 * b.M22,
          a.M20 * b.M00 + a.M21 * b.M10 + a.M22 * b.M20,
          a.M20 * b.M01 + a.M21 * b.M11 + a.M22 * b.M21,
          a.M20 * b.M02 + a.M21 * b.M12 + a.M22 * b.M22};
}

constexpr Float4x4 operator*(const Float4x4& a, const Float4x4& b) noexcept
{
  return {a.M00 * b.M00 + a.M01 * b.M10 + a.M02 * b.M20 + a.M03 * b.M30,
          a.M00 * b.M01 + a.M01 * b.M11 + a.M02 * b.M21 + a.M03 * b.M31,
          a.M00 * b.M02 + a.M01 * b.M12 + a.M02 * b.M22 + a.M03 * b.M32,
          a.M00 * b.M03 + a.M01 * b.M13 + a.M02 * b.M23 + a.M03 * b.M33,
          a.M10 * b.M00 + a.M11 * b.M10 + a.M12 * b.M20 + a.M13 * b.M30,
          a.M10 * b.M01 + a.M11 * b.M11 + a.M12 * b.M21 + a.M13 * b.M31,
          a.M10 * b.M02 + a.M11 * b.M12 + a.M12 * b.M22 + a.M13 * b.M32,
          a.M10 * b.M03 + a.M11 * b.M13 + a.M12 * b.M23 + a.M13 * b.M33,
          a.M20 * b.M00 + a.M21 * b.M10 + a.M22 * b.M20 + a.M23 * b.M30,
          a.M20 * b.M01 + a.M21 * b.M11 + a.M22 * b.M21 + a.M23 * b.M31,
          a.M20 * b.M02 + a.M21 * b.M12 + a.M22 * b.M22 + a.M23 * b.M32,
          a.M20 * b.M03 + a.M21 * b.M13 + a.M22 * b.M23 + a.M23 * b.M33,
          a.M30 * b.M00 + a.M31 * b.M10 + a.M32 * b.M20 + a.M33 * b.M30,
          a.M30 * b.M01 + a.M31 * b.M11 + a.M32 * b.M21 + a.M33 * b.M31,
          a.M30 * b.M02 + a.M31 * b.M12 + a.M32 * b.M22 + a.M33 * b.M32,
          a.M30 * b.M03 + a.M31 * b.M13 + a.M32 * b.M23 + a.M33 * b.M33};
}

// Matrix * vector
constexpr Float2 operator*(const Float2x2& m, const Float2& v) noexcept
{
  return {m.M00 * v.X + m.M01 * v.Y, m.M10 * v.X + m.M11 * v.Y};
}

constexpr Float3 operator*(const Float3x3& m, const Float3& v) noexcept
{
  return {m.M00 * v.X + m.M01 * v.Y + m.M02 * v.Z,
          m.M10 * v.X + m.M11 * v.Y + m.M12 * v.Z,
          m.M20 * v.X + m.M21 * v.Y + m.M22 * v.Z};
}

constexpr Float4 operator*(const Float4x4& m, const Float4& v) noexcept
{
  return {m.M00 * v.X + m.M01 * v.Y + m.M02 * v.Z + m.M03 * v.W,
          m.M10 * v.X + m.M11 * v.Y + m.M12 * v.Z + m.M13 * v.W,
          m.M20 * v.X + m.M21 * v.Y + m.M22 * v.Z + m.M23 * v.W,
          m.M30 * v.X + m.M31 * v.Y + m.M32 * v.Z + m.M33 * v.W};
}

[[nodiscard]] constexpr Float3 TransformPoint(const Float4x4& m,
                                              const Float3& v) noexcept
{
  const Float4 p = m * Float4 {v.X, v.Y, v.Z, 1.0f};
  return {p.X, p.Y, p.Z};
}

[[nodiscard]] constexpr Float3 TransformVector(const Float4x4& m,
                                               const Float3& v) noexcept
{
  const Float4 p = m * Float4 {v.X, v.Y, v.Z, 0.0f};
  return {p.X, p.Y, p.Z};
}

// Rotor conversion functions (declarations)
Float3x3 ToMatrix3(const Rotor& r) noexcept;
Float4x4 ToMatrix4(const Rotor& r) noexcept;
Rotor ToRotor(const Float3x3& m) noexcept;

using float2x2 = Float2x2;
using float3x3 = Float3x3;
using float4x4 = Float4x4;

}  // namespace gecko::math

#ifdef _MSC_VER
#pragma warning(pop)
#endif
