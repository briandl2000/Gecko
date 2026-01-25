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
  return 0;
}
