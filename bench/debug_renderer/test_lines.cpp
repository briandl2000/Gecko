/// @file
/// Bench: debug-renderer line throughput. Deterministic spiral
/// scene; min/mean/max bar chart compares runs.
///
/// This bench is designed to measure ONLY the line-rendering path:
///   - No std::rand, no std::cos/std::sin in the hot loop (lookup table).
///   - Scene is fully deterministic from the line index `i`.
///   - Per-line cost in the bench loop is ~6 integer ops + 2 muls + AddLine.
/// If `cpu_record` grows beyond ~10 ns / line on a desktop CPU, the
/// renderer (or AddLine) is the bottleneck, not the bench.

#include <gecko/bench/bench.h>
#include <gecko/bench/graphics_fixture.h>
#include <gecko/core/labels.h>
#include <gecko/core/scope.h>
#include <gecko/debug_renderer/debug_renderer_context.h>
#include <gecko/graphics/gpu_profiler.h>
#include <gecko/graphics/graphics_device.h>

#include <cmath>

namespace {

constexpr ::gecko::Label kLabel = ::gecko::MakeLabel("bench.debug_renderer");

// Trig table: 1024 entries on the unit circle. Indexed by an integer
// step so the inner loop has no std::cos/std::sin.
constexpr ::gecko::u32 kTrigSize = 1024;
constexpr ::gecko::u32 kTrigMask = kTrigSize - 1;

struct TrigTable
{
  ::gecko::f32 C[kTrigSize];
  ::gecko::f32 S[kTrigSize];
  TrigTable() noexcept
  {
    for (::gecko::u32 i = 0; i < kTrigSize; ++i)
    {
      const ::gecko::f32 a =
          static_cast<::gecko::f32>(i) / kTrigSize * 6.2831853F;
      C[i] = ::std::cos(a);
      S[i] = ::std::sin(a);
    }
  }
};

const TrigTable& Trig() noexcept
{
  static const TrigTable t;
  return t;
}

constexpr ::gecko::u32 kPaletteSize = 8;
constexpr ::gecko::math::float3 kPalette[kPaletteSize] = {
    {1.00F, 0.20F, 0.20F}, {1.00F, 0.55F, 0.10F}, {1.00F, 0.95F, 0.10F},
    {0.30F, 1.00F, 0.30F}, {0.20F, 0.85F, 1.00F}, {0.40F, 0.40F, 1.00F},
    {0.85F, 0.30F, 1.00F}, {1.00F, 0.40F, 0.85F},
};

/// Deterministic spiral: line `i` is a chord between two points on a
/// growing-radius spiral. Visually a rainbow swirl, computationally
/// just a few integer multiplies + table lookups + AddLine.
void BuildSpiral(::gecko::debug_renderer::DebugRendererContext& ctx,
                 ::gecko::u32 numLines, ::gecko::f32 w, ::gecko::f32 h)
{
  const auto& tr = Trig();
  const ::gecko::f32 cx = w * 0.5F;
  const ::gecko::f32 cy = h * 0.5F;
  const ::gecko::f32 maxR = (w < h ? w : h) * 0.45F;
  const ::gecko::f32 invN = 1.0F / static_cast<::gecko::f32>(numLines);
  // 7 turns over the whole sweep, stepping the angle by a coprime so
  // chord coverage is dense.
  constexpr ::gecko::u32 kAngleStep = 17u;
  constexpr ::gecko::u32 kChordSpan = 263u;  // prime, gives nice cross weave

  for (::gecko::u32 i = 0; i < numLines; ++i)
  {
    const ::gecko::u32 ia = (i * kAngleStep) & kTrigMask;
    const ::gecko::u32 ib = ((i + kChordSpan) * kAngleStep) & kTrigMask;
    const ::gecko::f32 r1 =
        maxR * (0.05F + 0.95F * static_cast<::gecko::f32>(i) * invN);
    const ::gecko::f32 r2 =
        maxR * (0.05F + 0.95F *
                            static_cast<::gecko::f32>(i + kChordSpan) * invN);
    const ::gecko::math::float2 p1 {cx + r1 * tr.C[ia], cy + r1 * tr.S[ia]};
    const ::gecko::math::float2 p2 {cx + r2 * tr.C[ib], cy + r2 * tr.S[ib]};
    ctx.DrawLine(p1, p2, kPalette[i & (kPaletteSize - 1)], 2.0F);
  }
}

constexpr ::gecko::u32 kStaticLines = 4000;

/// Shared driver. Reads `lines` from sweep args (default kStaticLines).
void RunDebugLines(::gecko::bench::State& s)
{
  const ::gecko::u32 argLines = static_cast<::gecko::u32>(s.Arg("lines"));
  const ::gecko::u32 numLines = argLines == 0 ? kStaticLines : argLines;
  ::gecko::bench::GraphicsFixture fx({.Title = "bench/debug_lines",
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
  // Size the per-frame line buffer for the max sweep we expect (256k).
  ::gecko::debug_renderer::DebugRendererContext ctx {512u * 1024u};
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

      BuildSpiral(ctx, numLines, fbW, fbH);
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

}  // namespace

/// Static comparison: 60 iters at kStaticLines lines, default bar chart.
static void lines_static(::gecko::bench::State& s)
{
  RunDebugLines(s);
}

GECKO_BENCH(lines_static)
    .Iterations(60)
    .Warmup(100)
    .MetricLabel(kLabel)
    .Description("Deterministic spiral scene at 4000 lines. Bar chart "
                 "compares min/mean/max for frame_total + cpu_record / "
                 "cmd_submit / cmd_execute / gpu_draw_lines across runs.");

/// Sweep: how does timing scale with the number of lines drawn?
static void lines_sweep(::gecko::bench::State& s)
{
  RunDebugLines(s);
}

GECKO_BENCH(lines_sweep)
    .Iterations(30)
    .Warmup(100)
    .Sweep("lines", {1000, 4000, 16000, 64000, 256000})
    .MetricLabel(kLabel)
    .Description("Same deterministic scene at varying line counts. "
                 "Renders a line chart (mean per run) with min/max band; "
                 "the slider scrubs to a bar view at one count.");

