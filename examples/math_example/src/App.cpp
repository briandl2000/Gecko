#include "App.h"

#include "gecko/core/format.h"
#include "gecko/math/math.h"
#include "gecko/platform/terminal.h"

namespace gecko::examples::math_example {

using gecko::f32;
using gecko::u32;
using gecko::math::Aabb2;
using gecko::math::Float2;
using gecko::math::Float3;
using gecko::math::Float4x4;
using gecko::math::Quat;
using gecko::math::RectI;

namespace {

template <typename... Arguments>
void Print(const char* format, const Arguments&... arguments) noexcept
{
  char text[512] {};
  FormatBuffer output {.Data = text, .Capacity = sizeof(text)};
  FormatTo(output, format, arguments...);
  platform::PrintLine(platform::TermStream::Stdout, text);
}

}  // namespace

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
  Print("--- Vector basics ---");

  Float3 position {2.0f, 3.0f, 1.0f};
  Float3 velocity {0.5f, -1.0f, 0.0f};
  Float3 acceleration {0.0f, -9.8f, 0.0f};
  const f32 dt = 1.0f / 60.0f;
  const f32 speed = 3.0f;

  position = position + velocity * speed * dt + acceleration * (0.5f * dt * dt);
  position[1] = 4.25f;

  const f32 distance = gecko::math::Length(position);
  const f32 alignment = gecko::math::Dot(position, velocity);
  const Float3 normal = gecko::math::Normalized(velocity);

  const Float2 facing {normal.X, normal.Y};
  const f32 perpendicular = gecko::math::Cross(facing, {1.0f, 0.0f});

  Print("pos=({:.2}, {:.2}, {:.2}) dist={:.2} align={:.2}", position.X, position.Y, position.Z, distance, alignment);
  Print("dir=({:.2}, {:.2}, {:.2}) cross={:.2}", normal.X, normal.Y, normal.Z, perpendicular);
  Print("");
}

void App::RunMatrixTransforms()
{
  Print("--- Matrix transforms ---");

  const Float4x4 transform = Float4x4::Translation({1.0f, 2.0f, 3.0f}) * Float4x4::Scale({2.0f, 2.0f, 1.0f});

  const Float3 point {2.0f, 4.25f, 1.0f};
  const Float3 dir = gecko::math::Normalized(Float3 {0.5f, -1.0f, 0.0f});

  const Float3 worldPos = gecko::math::TransformPoint(transform, point);
  const Float3 worldDir = gecko::math::TransformVector(transform, dir);

  const Float4x4 rotZ = Float4x4::RotationZ(gecko::math::ToRadians(90.0f));
  const Float3 rotated = gecko::math::TransformPoint(rotZ, {1.0f, 0.0f, 0.0f});

  Print("worldPos=({:.2}, {:.2}, {:.2}) worldDir=({:.2}, {:.2}, {:.2})", worldPos.X, worldPos.Y, worldPos.Z, worldDir.X,
        worldDir.Y, worldDir.Z);
  Print("(1,0,0) rotated 90 deg around Z = ({:.2}, {:.2}, {:.2})", rotated.X, rotated.Y, rotated.Z);
  Print("");
}

void App::RunCameraMatrices()
{
  Print("--- Camera matrices ---");

  const Float4x4 view = Float4x4::LookAt({0.0f, 0.0f, 5.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
  const Float4x4 proj = Float4x4::Perspective(gecko::math::ToRadians(60.0f), 16.0f / 9.0f, 0.1f, 100.0f);
  const Float4x4 viewProj = proj * view;

  Print("ViewProj M00={:.2} M11={:.2}", viewProj.M00, viewProj.M11);
  Print("");
}

void App::RunAabbAndRect()
{
  Print("--- AABB and Rect ---");

  Aabb2 screenRect {{0.0f, 0.0f}, {1920.0f, 1080.0f}};
  Aabb2 spriteRect {{40.0f, 60.0f}, {120.0f, 140.0f}};

  const bool onScreen = gecko::math::Contains(screenRect, {800.0f, 400.0f});
  const bool visible = gecko::math::Intersects(screenRect, spriteRect);
  const Float2 screenSize = gecko::math::Size(screenRect);
  const Float2 screenCenter = gecko::math::Center(screenRect);

  Aabb2 box1 {{10.0f, 10.0f}, {50.0f, 50.0f}};
  Aabb2 box2 {{30.0f, 30.0f}, {70.0f, 70.0f}};
  const Aabb2 expanded = gecko::math::Expand(box1, 5.0f);
  const Aabb2 combined = gecko::math::Union(box1, box2);
  const Float2 clamped = gecko::math::Clamp(box1, {5.0f, 100.0f});

  RectI monitorRect {{0, 0}, {2560, 1440}};
  const auto monitorSize = gecko::math::Size(monitorRect);
  const u32 pixelCount = static_cast<u32>(monitorSize.X) * static_cast<u32>(monitorSize.Y);

  Print("screen={:.0}x{:.0} center=({:.1}, {:.1}) onScreen={}", screenSize.X, screenSize.Y, screenCenter.X,
        screenCenter.Y, onScreen);
  Print("sprite visible={}", visible);
  Print("expanded box: ({:.0},{:.0})-({:.0},{:.0})", expanded.Min.X, expanded.Min.Y, expanded.Max.X, expanded.Max.Y);
  Print("union box: ({:.0},{:.0})-({:.0},{:.0})", combined.Min.X, combined.Min.Y, combined.Max.X, combined.Max.Y);
  Print("clamped point: ({:.0}, {:.0})", clamped.X, clamped.Y);
  Print("monitor={}x{} pixels={}", monitorSize.X, monitorSize.Y, pixelCount);
  Print("");
}

void App::RunQuaternions()
{
  Print("--- Quaternions ---");

  const Quat q1 = Quat::AxisAngle({0.0f, 0.0f, 1.0f}, gecko::math::ToRadians(90.0f));
  const Float3 vec {1.0f, 0.0f, 0.0f};
  const Float3 rotatedVec = gecko::math::Rotate(q1, vec);

  const Quat q2 = Quat::AxisAngle({1.0f, 0.0f, 0.0f}, gecko::math::ToRadians(45.0f));
  const Quat composed = q2 * q1;
  const Float3 doubleRotated = gecko::math::Rotate(composed, vec);

  const Quat fromTo = Quat::FromTo({1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f});
  const Float3 aligned = gecko::math::Rotate(fromTo, {1.0f, 0.0f, 0.0f});

  const Quat qStart = Quat::AxisAngle({0.0f, 1.0f, 0.0f}, 0.0f);
  const Quat qEnd = Quat::AxisAngle({0.0f, 1.0f, 0.0f}, gecko::math::ToRadians(180.0f));
  const Quat mid = gecko::math::Slerp(qStart, qEnd, 0.5f);
  const Float3 halfRotated = gecko::math::Rotate(mid, {1.0f, 0.0f, 0.0f});

  Print("(1,0,0) rotated 90 deg around Z: ({:.2}, {:.2}, {:.2})", rotatedVec.X, rotatedVec.Y, rotatedVec.Z);
  Print("composed rotation: ({:.2}, {:.2}, {:.2})", doubleRotated.X, doubleRotated.Y, doubleRotated.Z);
  Print("FromTo aligns (1,0,0) to: ({:.2}, {:.2}, {:.2})", aligned.X, aligned.Y, aligned.Z);
  Print("Slerp halfway 0->180 deg: ({:.2}, {:.2}, {:.2})", halfRotated.X, halfRotated.Y, halfRotated.Z);
  Print("");
}

void App::RunUtilities()
{
  Print("--- Utilities ---");

  const f32 angle = gecko::math::ToRadians(45.0f);
  const Float3 a {1.0f, 5.0f, 2.0f};
  const Float3 b {4.0f, 2.0f, 3.0f};
  const Float3 minVec = gecko::math::Min(a, b);
  const Float3 maxVec = gecko::math::Max(a, b);
  const Float3 lerped = gecko::math::Lerp(a, b, 0.5f);

  Print("45 deg = {:.4} rad", angle);
  Print("Min(a,b) = ({:.1}, {:.1}, {:.1})", minVec.X, minVec.Y, minVec.Z);
  Print("Max(a,b) = ({:.1}, {:.1}, {:.1})", maxVec.X, maxVec.Y, maxVec.Z);
  Print("Lerp(a,b,0.5) = ({:.1}, {:.1}, {:.1})", lerped.X, lerped.Y, lerped.Z);
}

}  // namespace gecko::examples::math_example
