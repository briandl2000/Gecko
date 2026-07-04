#pragma once

/// @file
/// Position/rotation/scale transform helper for common game-object transforms.

#include "gecko/math/matrix.h"
#include "gecko/math/quat.h"

namespace gecko::math {

struct Transform
{
  Float3 Position {};
  Quat Rotation {};
  Float3 Scale {1.0f, 1.0f, 1.0f};

  constexpr Transform() noexcept = default;
  constexpr Transform(const Float3& position, const Quat& rotation = Quat::Identity(),
                      const Float3& scale = {1.0f, 1.0f, 1.0f}) noexcept
      : Position(position), Rotation(rotation), Scale(scale)
  {}

  [[nodiscard]] static constexpr Transform Identity() noexcept
  {
    return {};
  }
};

[[nodiscard]] inline Float4x4 ToMatrix(const Transform& transform) noexcept
{
  return Float4x4::Translation(transform.Position) * ToMatrix4(transform.Rotation) * Float4x4::Scale(transform.Scale);
}

[[nodiscard]] inline Transform Inversed(const Transform& transform) noexcept
{
  const Float3 invScale {1.0f / transform.Scale.X, 1.0f / transform.Scale.Y, 1.0f / transform.Scale.Z};
  const Quat invRotation = Inverse(transform.Rotation);
  const Float3 invPosition = Rotate(invRotation, transform.Position * -1.0f) * invScale;
  return {invPosition, invRotation, invScale};
}

[[nodiscard]] inline Float3 TransformPoint(const Transform& transform, const Float3& point) noexcept
{
  return Rotate(transform.Rotation, point * transform.Scale) + transform.Position;
}

[[nodiscard]] inline Float3 TransformVector(const Transform& transform, const Float3& vector) noexcept
{
  return Rotate(transform.Rotation, vector * transform.Scale);
}

[[nodiscard]] inline Transform operator*(const Transform& parent, const Transform& child) noexcept
{
  return {TransformPoint(parent, child.Position), parent.Rotation * child.Rotation, parent.Scale * child.Scale};
}

}  // namespace gecko::math
