#include "gecko/math/math.h"

#include <cstdio>

int main()
{
  using ::gecko::f32;
  using ::gecko::u32;
  using ::gecko::math::Aabb2;
  using ::gecko::math::Float2;
  using ::gecko::math::Float3;
  using ::gecko::math::Float4x4;
  using ::gecko::math::RectI;

  // Basic vectors + indexing
  Float3 position {2.0f, 3.0f, 1.0f};
  Float3 velocity {0.5f, -1.0f, 0.0f};
  Float3 acceleration {0.0f, -9.8f, 0.0f};
  const f32 dt = 1.0f / 60.0f;
  const f32 speed = 3.0f;

  position = position + velocity * speed * dt + acceleration * (0.5f * dt * dt);
  position[1] = 4.25f;  // index access

  const f32 distance = ::gecko::math::Length(position);
  const f32 alignment = ::gecko::math::Dot(position, velocity);
  const Float3 normal = ::gecko::math::Normalized(velocity);

  // 2D helpers
  const Float2 facing {normal.X, normal.Y};
  const f32 perpendicular = ::gecko::math::Cross(facing, {1.0f, 0.0f});

  // Matrices and transforms
  const Float4x4 transform = Float4x4::Translation({1.0f, 2.0f, 3.0f}) *
                             Float4x4::Scale({2.0f, 2.0f, 1.0f});
  const Float3 worldPos = ::gecko::math::TransformPoint(transform, position);
  const Float3 worldDir = ::gecko::math::TransformVector(transform, normal);

  // AABB usage
  Aabb2 screenRect {{0.0f, 0.0f}, {1920.0f, 1080.0f}};
  const bool onScreen =
      ::gecko::math::Contains(screenRect, {worldPos.X, worldPos.Y});
  const auto screenSize = ::gecko::math::Size(screenRect);
  const auto screenCenter = ::gecko::math::Center(screenRect);

  Aabb2 spriteRect {{40.0f, 60.0f}, {120.0f, 140.0f}};
  const bool visible = ::gecko::math::Intersects(screenRect, spriteRect);
  const Float2 spriteSize = ::gecko::math::Size(spriteRect);

  // Integer rects for monitor/screen metrics
  RectI monitorRect {{0, 0}, {2560, 1440}};
  const auto monitorSize = ::gecko::math::Size(monitorRect);
  const u32 pixelCount =
      static_cast<u32>(monitorSize.X) * static_cast<u32>(monitorSize.Y);

  ::std::printf("pos=(%.2f, %.2f, %.2f) dist=%.2f align=%.2f\n", position.X,
                position.Y, position.Z, distance, alignment);
  ::std::printf("dir=(%.2f, %.2f, %.2f) cross=%.2f\n", normal.X, normal.Y,
                normal.Z, perpendicular);
  ::std::printf("worldPos=(%.2f, %.2f, %.2f) worldDir=(%.2f, %.2f, %.2f)\n",
                worldPos.X, worldPos.Y, worldPos.Z, worldDir.X, worldDir.Y,
                worldDir.Z);
  ::std::printf("screen=%0.0fx%0.0f center=(%.1f, %.1f) onScreen=%s\n",
                screenSize.X, screenSize.Y, screenCenter.X, screenCenter.Y,
                onScreen ? "true" : "false");
  ::std::printf("sprite=%0.0fx%0.0f visible=%s\n", spriteSize.X, spriteSize.Y,
                visible ? "true" : "false");
  ::std::printf("monitor=%dx%d pixels=%u\n", monitorSize.X, monitorSize.Y,
                pixelCount);

  // NEW: Test utility functions
  ::std::printf("\n--- Utility Functions ---\n");
  const f32 angle = ::gecko::math::ToRadians(45.0f);
  ::std::printf("45 degrees = %.4f radians\n", angle);

  const Float3 a {1.0f, 5.0f, 2.0f};
  const Float3 b {4.0f, 2.0f, 3.0f};
  const Float3 minVec = ::gecko::math::Min(a, b);
  const Float3 maxVec = ::gecko::math::Max(a, b);
  const Float3 lerped = ::gecko::math::Lerp(a, b, 0.5f);
  ::std::printf("Min(a,b) = (%.1f, %.1f, %.1f)\n", minVec.X, minVec.Y,
                minVec.Z);
  ::std::printf("Max(a,b) = (%.1f, %.1f, %.1f)\n", maxVec.X, maxVec.Y,
                maxVec.Z);
  ::std::printf("Lerp(a,b,0.5) = (%.1f, %.1f, %.1f)\n", lerped.X, lerped.Y,
                lerped.Z);

  // NEW: Test rotation matrices
  ::std::printf("\n--- Rotation Matrices ---\n");
  const Float4x4 rotZ = Float4x4::RotationZ(::gecko::math::ToRadians(90.0f));
  const Float3 point {1.0f, 0.0f, 0.0f};
  const Float3 rotated = ::gecko::math::TransformPoint(rotZ, point);
  ::std::printf("Point (1,0,0) rotated 90° around Z = (%.2f, %.2f, %.2f)\n",
                rotated.X, rotated.Y, rotated.Z);

  // NEW: Test camera matrices
  ::std::printf("\n--- Camera Matrices ---\n");
  const Float4x4 view = Float4x4::LookAt({0.0f, 0.0f, 5.0f}, {0.0f, 0.0f, 0.0f},
                                         {0.0f, 1.0f, 0.0f});
  const Float4x4 proj = Float4x4::Perspective(::gecko::math::ToRadians(60.0f),
                                              16.0f / 9.0f, 0.1f, 100.0f);
  const Float4x4 viewProj = proj * view;
  ::std::printf("ViewProj matrix M00=%.2f M11=%.2f\n", viewProj.M00,
                viewProj.M11);

  // NEW: Test AABB operations
  ::std::printf("\n--- AABB Operations ---\n");
  Aabb2 box1 {{10.0f, 10.0f}, {50.0f, 50.0f}};
  Aabb2 box2 {{30.0f, 30.0f}, {70.0f, 70.0f}};
  const Aabb2 expanded = ::gecko::math::Expand(box1, 5.0f);
  const Aabb2 combined = ::gecko::math::Union(box1, box2);
  const Float2 clamped = ::gecko::math::Clamp(box1, {5.0f, 100.0f});
  ::std::printf("Expanded box: (%.0f,%.0f)-(%.0f,%.0f)\n", expanded.Min.X,
                expanded.Min.Y, expanded.Max.X, expanded.Max.Y);
  ::std::printf("Union box: (%.0f,%.0f)-(%.0f,%.0f)\n", combined.Min.X,
                combined.Min.Y, combined.Max.X, combined.Max.Y);
  ::std::printf("Clamped point: (%.0f, %.0f)\n", clamped.X, clamped.Y);

  // Test Quaternions (rotation representation)
  ::std::printf("\n--- Quaternions ---\n");
  using ::gecko::math::Quat;

  // Create quaternion from axis-angle
  const Quat q1 =
      Quat::AxisAngle({0.0f, 0.0f, 1.0f}, ::gecko::math::ToRadians(90.0f));
  const Float3 vec {1.0f, 0.0f, 0.0f};
  const Float3 rotatedVec = ::gecko::math::Rotate(q1, vec);
  ::std::printf(
      "Vector (1,0,0) rotated 90° around Z-axis: (%.2f, %.2f, %.2f)\n",
      rotatedVec.X, rotatedVec.Y, rotatedVec.Z);

  // Quaternion composition (apply q2 after q1)
  const Quat q2 =
      Quat::AxisAngle({1.0f, 0.0f, 0.0f}, ::gecko::math::ToRadians(45.0f));
  const Quat combined_quat = q2 * q1;  // Compose rotations
  const Float3 doubleRotated = ::gecko::math::Rotate(combined_quat, vec);
  ::std::printf("After composing two rotations: (%.2f, %.2f, %.2f)\n",
                doubleRotated.X, doubleRotated.Y, doubleRotated.Z);

  // Quaternion from one vector to another
  const Float3 from {1.0f, 0.0f, 0.0f};
  const Float3 to {0.0f, 1.0f, 0.0f};
  const Quat fromTo = Quat::FromTo(from, to);
  const Float3 aligned = ::gecko::math::Rotate(fromTo, from);
  ::std::printf("FromTo quaternion aligns (1,0,0) to: (%.2f, %.2f, %.2f)\n",
                aligned.X, aligned.Y, aligned.Z);

  // Spherical linear interpolation
  const Quat q3 = Quat::AxisAngle({0.0f, 1.0f, 0.0f}, 0.0f);
  const Quat q4 =
      Quat::AxisAngle({0.0f, 1.0f, 0.0f}, ::gecko::math::ToRadians(180.0f));
  const Quat midQuat = ::gecko::math::Slerp(q3, q4, 0.5f);
  const Float3 halfRotated =
      ::gecko::math::Rotate(midQuat, {1.0f, 0.0f, 0.0f});
  ::std::printf("Slerp halfway between 0° and 180°: (%.2f, %.2f, %.2f)\n",
                halfRotated.X, halfRotated.Y, halfRotated.Z);

  return 0;
}
