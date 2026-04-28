#include "App.h"

#include "gecko/math/math.h"

#include <cstdio>

namespace gecko::examples::math_example {

using ::gecko::f32;
using ::gecko::u32;
using ::gecko::math::Aabb2;
using ::gecko::math::Float2;
using ::gecko::math::Float3;
using ::gecko::math::Float4x4;
using ::gecko::math::Quat;
using ::gecko::math::RectI;

int App::Run()
{
  RunVectorBasics();
  RunMatrixTransforms();
  RunCameraMatrices();
  RunAabbAndRect();
  RunQuaternions();
  RunUtilities();
  return 0;
}

void App::RunVectorBasics()
{
  ::std::printf("--- Vector basics ---\n");

  Float3 position {2.0f, 3.0f, 1.0f};
  Float3 velocity {0.5f, -1.0f, 0.0f};
  Float3 acceleration {0.0f, -9.8f, 0.0f};
  const f32 dt = 1.0f / 60.0f;
  const f32 speed = 3.0f;

  position = position + velocity * speed * dt + acceleration * (0.5f * dt * dt);
  position[1] = 4.25f;

  const f32 distance = ::gecko::math::Length(position);
  const f32 alignment = ::gecko::math::Dot(position, velocity);
  const Float3 normal = ::gecko::math::Normalized(velocity);

  const Float2 facing {normal.X, normal.Y};
  const f32 perpendicular = ::gecko::math::Cross(facing, {1.0f, 0.0f});

  ::std::printf("pos=(%.2f, %.2f, %.2f) dist=%.2f align=%.2f\n", position.X,
                position.Y, position.Z, distance, alignment);
  ::std::printf("dir=(%.2f, %.2f, %.2f) cross=%.2f\n\n", normal.X, normal.Y,
                normal.Z, perpendicular);
}

void App::RunMatrixTransforms()
{
  ::std::printf("--- Matrix transforms ---\n");

  const Float4x4 transform = Float4x4::Translation({1.0f, 2.0f, 3.0f}) *
                             Float4x4::Scale({2.0f, 2.0f, 1.0f});

  const Float3 point {2.0f, 4.25f, 1.0f};
  const Float3 dir = ::gecko::math::Normalized(Float3 {0.5f, -1.0f, 0.0f});

  const Float3 worldPos = ::gecko::math::TransformPoint(transform, point);
  const Float3 worldDir = ::gecko::math::TransformVector(transform, dir);

  const Float4x4 rotZ = Float4x4::RotationZ(::gecko::math::ToRadians(90.0f));
  const Float3 rotated =
      ::gecko::math::TransformPoint(rotZ, {1.0f, 0.0f, 0.0f});

  ::std::printf("worldPos=(%.2f, %.2f, %.2f) worldDir=(%.2f, %.2f, %.2f)\n",
                worldPos.X, worldPos.Y, worldPos.Z, worldDir.X, worldDir.Y,
                worldDir.Z);
  ::std::printf("(1,0,0) rotated 90 deg around Z = (%.2f, %.2f, %.2f)\n\n",
                rotated.X, rotated.Y, rotated.Z);
}

void App::RunCameraMatrices()
{
  ::std::printf("--- Camera matrices ---\n");

  const Float4x4 view = Float4x4::LookAt({0.0f, 0.0f, 5.0f}, {0.0f, 0.0f, 0.0f},
                                         {0.0f, 1.0f, 0.0f});
  const Float4x4 proj = Float4x4::Perspective(::gecko::math::ToRadians(60.0f),
                                              16.0f / 9.0f, 0.1f, 100.0f);
  const Float4x4 viewProj = proj * view;

  ::std::printf("ViewProj M00=%.2f M11=%.2f\n\n", viewProj.M00, viewProj.M11);
}

void App::RunAabbAndRect()
{
  ::std::printf("--- AABB and Rect ---\n");

  Aabb2 screenRect {{0.0f, 0.0f}, {1920.0f, 1080.0f}};
  Aabb2 spriteRect {{40.0f, 60.0f}, {120.0f, 140.0f}};

  const bool onScreen = ::gecko::math::Contains(screenRect, {800.0f, 400.0f});
  const bool visible = ::gecko::math::Intersects(screenRect, spriteRect);
  const Float2 screenSize = ::gecko::math::Size(screenRect);
  const Float2 screenCenter = ::gecko::math::Center(screenRect);

  Aabb2 box1 {{10.0f, 10.0f}, {50.0f, 50.0f}};
  Aabb2 box2 {{30.0f, 30.0f}, {70.0f, 70.0f}};
  const Aabb2 expanded = ::gecko::math::Expand(box1, 5.0f);
  const Aabb2 combined = ::gecko::math::Union(box1, box2);
  const Float2 clamped = ::gecko::math::Clamp(box1, {5.0f, 100.0f});

  RectI monitorRect {{0, 0}, {2560, 1440}};
  const auto monitorSize = ::gecko::math::Size(monitorRect);
  const u32 pixelCount =
      static_cast<u32>(monitorSize.X) * static_cast<u32>(monitorSize.Y);

  ::std::printf("screen=%0.0fx%0.0f center=(%.1f, %.1f) onScreen=%s\n",
                screenSize.X, screenSize.Y, screenCenter.X, screenCenter.Y,
                onScreen ? "true" : "false");
  ::std::printf("sprite visible=%s\n", visible ? "true" : "false");
  ::std::printf("expanded box: (%.0f,%.0f)-(%.0f,%.0f)\n", expanded.Min.X,
                expanded.Min.Y, expanded.Max.X, expanded.Max.Y);
  ::std::printf("union box: (%.0f,%.0f)-(%.0f,%.0f)\n", combined.Min.X,
                combined.Min.Y, combined.Max.X, combined.Max.Y);
  ::std::printf("clamped point: (%.0f, %.0f)\n", clamped.X, clamped.Y);
  ::std::printf("monitor=%dx%d pixels=%u\n\n", monitorSize.X, monitorSize.Y,
                pixelCount);
}

void App::RunQuaternions()
{
  ::std::printf("--- Quaternions ---\n");

  const Quat q1 =
      Quat::AxisAngle({0.0f, 0.0f, 1.0f}, ::gecko::math::ToRadians(90.0f));
  const Float3 vec {1.0f, 0.0f, 0.0f};
  const Float3 rotatedVec = ::gecko::math::Rotate(q1, vec);

  const Quat q2 =
      Quat::AxisAngle({1.0f, 0.0f, 0.0f}, ::gecko::math::ToRadians(45.0f));
  const Quat composed = q2 * q1;
  const Float3 doubleRotated = ::gecko::math::Rotate(composed, vec);

  const Quat fromTo = Quat::FromTo({1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
  const Float3 aligned = ::gecko::math::Rotate(fromTo, {1.0f, 0.0f, 0.0f});

  const Quat qStart = Quat::AxisAngle({0.0f, 1.0f, 0.0f}, 0.0f);
  const Quat qEnd =
      Quat::AxisAngle({0.0f, 1.0f, 0.0f}, ::gecko::math::ToRadians(180.0f));
  const Quat mid = ::gecko::math::Slerp(qStart, qEnd, 0.5f);
  const Float3 halfRotated = ::gecko::math::Rotate(mid, {1.0f, 0.0f, 0.0f});

  ::std::printf("(1,0,0) rotated 90 deg around Z: (%.2f, %.2f, %.2f)\n",
                rotatedVec.X, rotatedVec.Y, rotatedVec.Z);
  ::std::printf("composed rotation: (%.2f, %.2f, %.2f)\n", doubleRotated.X,
                doubleRotated.Y, doubleRotated.Z);
  ::std::printf("FromTo aligns (1,0,0) to: (%.2f, %.2f, %.2f)\n", aligned.X,
                aligned.Y, aligned.Z);
  ::std::printf("Slerp halfway 0->180 deg: (%.2f, %.2f, %.2f)\n\n",
                halfRotated.X, halfRotated.Y, halfRotated.Z);
}

void App::RunUtilities()
{
  ::std::printf("--- Utilities ---\n");

  const f32 angle = ::gecko::math::ToRadians(45.0f);
  const Float3 a {1.0f, 5.0f, 2.0f};
  const Float3 b {4.0f, 2.0f, 3.0f};
  const Float3 minVec = ::gecko::math::Min(a, b);
  const Float3 maxVec = ::gecko::math::Max(a, b);
  const Float3 lerped = ::gecko::math::Lerp(a, b, 0.5f);

  ::std::printf("45 deg = %.4f rad\n", angle);
  ::std::printf("Min(a,b) = (%.1f, %.1f, %.1f)\n", minVec.X, minVec.Y,
                minVec.Z);
  ::std::printf("Max(a,b) = (%.1f, %.1f, %.1f)\n", maxVec.X, maxVec.Y,
                maxVec.Z);
  ::std::printf("Lerp(a,b,0.5) = (%.1f, %.1f, %.1f)\n", lerped.X, lerped.Y,
                lerped.Z);
}

}  // namespace gecko::examples::math_example
