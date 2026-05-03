/// @file
/// Bench: per-iteration debug-renderer line-drawing throughput, no
/// sweep -> dynamic / line-graph view in the report.

#include <gecko/bench/bench.h>
#include <gecko/bench/graphics_fixture.h>
#include <gecko/core/labels.h>
#include <gecko/core/scope.h>
#include <gecko/core/utility/random.h>
#include <gecko/debug_renderer/debug_renderer_context.h>
#include <gecko/graphics/graphics_device.h>
#include <gecko/graphics/gpu_profiler.h>

#include <cmath>

namespace {

constexpr ::gecko::Label kLabel = ::gecko::MakeLabel("bench.debug_renderer");
constexpr ::gecko::u32 kCircles = 4000;
constexpr ::gecko::u32 kSegments = 32;

void DrawCircle(::gecko::debug_renderer::DebugRendererContext& ctx,
                ::gecko::math::float2 center, ::gecko::f32 radius,
                ::gecko::math::float3 color, ::gecko::f32 thickness)
{
  for (::gecko::u32 i = 0; i < kSegments; ++i)
  {
    const ::gecko::f32 a1 =
        static_cast<::gecko::f32>(i) / kSegments * 6.2831853F;
    const ::gecko::f32 a2 =
        static_cast<::gecko::f32>(i + 1) / kSegments * 6.2831853F;
    const ::gecko::math::float2 p1 {center.X + radius * ::std::cos(a1),
                                    center.Y + radius * ::std::sin(a1)};
    const ::gecko::math::float2 p2 {center.X + radius * ::std::cos(a2),
                                    center.Y + radius * ::std::sin(a2)};
    ctx.DrawLine(p1, p2, color, thickness);
  }
}

}  // namespace

/// Draw a fixed number of circles per frame. Profile macros inside
/// the loop become per-metric graphs in the report (line-chart view).
static void debug_lines_dynamic(::gecko::bench::State& s)
{
  ::gecko::bench::GraphicsFixture fx({.Title = "bench/debug_lines_dynamic",
                                      .Width = 1280,
                                      .Height = 720,
                                      .VSync = false});
  if (!fx.IsValid())
  {
    s.Abort("graphics setup failed");
    return;
  }

  auto* device = fx.Device();
  auto* sampler = fx.GpuSampler();
  ::gecko::debug_renderer::DebugRendererContext ctx;
  if (!ctx.IsValid())
  {
    s.Abort("DebugRendererContext setup failed");
    return;
  }

  for (auto _ : s)
  {
    fx.PumpEvents();
    auto frame = fx.BeginFrame();
    if (!frame.Valid)
      continue;

    auto cmd = device->CreateGraphicsCommandList();
    cmd->Begin();
    if (sampler)
      sampler->BeginFrame(*cmd);

    {
      GECKO_PROFILE_NORMAL_NAMED(kLabel, "cpu_record");
      ctx.NewFrame();
      const auto fbW = static_cast<::gecko::f32>(frame.BackBuffer.Desc.Width);
      const auto fbH = static_cast<::gecko::f32>(frame.BackBuffer.Desc.Height);

      ::gecko::graphics::ClearValue clear =
          ::gecko::graphics::ClearValue::RenderTarget(0.05F, 0.05F, 0.08F,
                                                     1.0F);
      cmd->BeginRendering(frame.BackBuffer, &clear);
      ctx.SetFrame(frame.BackBuffer);

      for (::gecko::u32 i = 0; i < kCircles; ++i)
      {
        const ::gecko::math::float2 center {::gecko::RandomF32(0.0F, fbW),
                                            ::gecko::RandomF32(0.0F, fbH)};
        DrawCircle(ctx, center, ::gecko::RandomF32(20.0F, 100.0F),
                   {1.0F, 0.0F, 0.0F}, 2.0F);
      }
    }

    {
      GECKO_PROFILE_NORMAL_NAMED(kLabel, "cmd_submit");
      if (sampler)
      {
        GECKO_GPU_PROF_SCOPE(*sampler, *cmd, kLabel, "gpu_draw_lines");
        ctx.Submit(cmd.get());
      }
      else
      {
        ctx.Submit(cmd.get());
      }
      cmd->EndRendering();
      ctx.EndFrame();
    }

    if (sampler)
      sampler->EndFrame(*cmd);
    cmd->End();

    {
      GECKO_PROFILE_NORMAL_NAMED(kLabel, "cmd_execute");
      device->ExecuteGraphicsCommandList(::std::move(cmd));
    }

    fx.Present(frame);
  }
}

GECKO_BENCH(debug_lines_dynamic)
    .Iterations(120)
    .Warmup(10)
    .Description("Fixed 4000 circles/frame; line-chart of frame_total + "
                 "cpu_record / cmd_submit / cmd_execute / gpu_draw_lines "
                 "across iterations.");
