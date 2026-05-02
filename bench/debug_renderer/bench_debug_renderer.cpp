/// @file
/// Benchmarks for the debug renderer's 2D line submission path.
///
/// Cases:
///   - debug_lines: 4000 random circles per frame, total frame time +
///     command-list record time + draw-call dispatch time.

#include <gecko/bench/bench.h>
#include <gecko/bench/graphics_fixture.h>
#include <gecko/core/utility/random.h>
#include <gecko/debug_renderer/debug_renderer_context.h>
#include <gecko/graphics/command_list.h>
#include <gecko/graphics/graphics_device.h>
#include <gecko/math/math.h>
#include <gecko/platform/input.h>

#include <cmath>

namespace {

using namespace ::gecko;
using namespace ::gecko::graphics;

void DrawCircle(debug_renderer::DebugRendererContext& ctx,
                ::gecko::math::float2 center, f32 radius,
                ::gecko::math::float3 color, f32 thickness)
{
  constexpr u32 kSegments = 32;
  for (u32 i = 0; i < kSegments; ++i)
  {
    const f32 a1 =
        (static_cast<f32>(i) / kSegments) * ::gecko::math::TwoPi;
    const f32 a2 =
        (static_cast<f32>(i + 1) / kSegments) * ::gecko::math::TwoPi;
    const ::gecko::math::float2 p1 {center.X + radius * ::std::cos(a1),
                                    center.Y + radius * ::std::sin(a1)};
    const ::gecko::math::float2 p2 {center.X + radius * ::std::cos(a2),
                                    center.Y + radius * ::std::sin(a2)};
    ctx.DrawLine(p1, p2, color, thickness);
  }
}

void debug_lines(::gecko::bench::State& s)
{
  ::gecko::bench::GraphicsFixture fx({.Title = "bench/debug_lines",
                                      .Width = 1280,
                                      .Height = 720,
                                      .Visible = true,
                                      .VSync = false});
  if (!fx.IsValid())
  {
    s.Abort("graphics setup failed");
    return;
  }

  ::gecko::Shared<debug_renderer::DebugRendererContext> ctx =
      ::gecko::CreateShared<debug_renderer::DebugRendererContext>();
  if (!ctx || !ctx->IsValid())
  {
    s.Abort("debug renderer context failed");
    return;
  }

  auto* device = fx.Device();
  const auto fbDesc = fx.Swapchain().Desc;
  const auto fbW = static_cast<f32>(fbDesc.Width);
  const auto fbH = static_cast<f32>(fbDesc.Height);

  const i64 numCircles = 4000;
  s.Counter("circles_per_frame", numCircles);
  s.Counter("lines_per_frame", numCircles * 32);

  for (auto _ : s)
  {
    fx.PumpEvents();

    FrameContext frame = fx.BeginFrame();
    if (!frame.Valid)
      continue;

    {
      ::gecko::bench::State::ScopedSection sec(s, "cpu_record");
      ctx->NewFrame();

      for (i64 i = 0; i < numCircles; ++i)
      {
        const ::gecko::math::float2 c {::gecko::RandomF32(0.0F, fbW),
                                       ::gecko::RandomF32(0.0F, fbH)};
        DrawCircle(*ctx, c, ::gecko::RandomF32(20.0F, 100.0F),
                   {1.0F, 0.0F, 0.0F}, 2.0F);
      }
    }

    auto cmd = device->CreateGraphicsCommandList();
    cmd->Begin();

    ::gecko::graphics::ClearValue clear =
        ::gecko::graphics::ClearValue::RenderTarget(0.05F, 0.05F, 0.08F, 1.0F);
    cmd->BeginRendering(frame.BackBuffer, &clear);
    ctx->SetFrame(frame.BackBuffer);

    {
      ::gecko::bench::State::ScopedSection sec(s, "cmd_submit");
      ctx->Submit(cmd.get());
    }

    cmd->EndRendering();
    ctx->EndFrame();
    cmd->End();

    {
      ::gecko::bench::State::ScopedSection sec(s, "cmd_execute");
      device->ExecuteGraphicsCommandList(::std::move(cmd));
    }

    fx.Present(frame);
  }
}

}  // namespace

GECKO_BENCH(debug_lines)
    .Iterations(120)
    .Warmup(10)
    .Description("4000 random circles (32 segments each) per frame; CPU "
                 "record / GPU submit / cmd execute timed separately.");
