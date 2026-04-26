#pragma once

#include "gecko/core/api.h"
#include "gecko/core/labels.h"
#include "gecko/core/ptr.h"
#include "gecko/core/services/profiler.h"
#include "gecko/core/types.h"
#include "gecko/graphics/command_list.h"

namespace gecko::graphics {

// ── GPU profiler ─────────────────────────────────────────────────────────
//
// Records GPU-side timing for a single render queue. Pair every BeginZone
// with an EndZone on the same command list; nest as desired. Frames must
// be bracketed with BeginFrame / EndFrame so the sampler can rotate its
// timestamp pools and resolve completed frames into IProfiler events.
//
// Threading: a single IGpuSampler instance is owned by the thread that
// records the queue's command lists for the frame. Calls into the
// profiler happen from EndFrame on that same thread.

struct GpuSamplerDesc
{
  // Worst-case zones per frame (each zone consumes 2 timestamps).
  u32 MaxZonesPerFrame {64};

  // Number of frames to keep in flight before resolving. Must be >= the
  // graphics device's MaxFramesInFlight so timestamps are guaranteed
  // available when we read them.
  u32 FramesInFlight {3};

  // Synthetic thread id for the GPU rows in the trace. Defaults to a
  // sentinel that is unlikely to collide with any OS thread id.
  u32 GpuThreadId {0xFFFF0001u};

  // Name registered via RegisterThreadProfilerName for GpuThreadId.
  const char* GpuThreadName {"GPU"};
};

class IGpuSampler
{
public:
  virtual ~IGpuSampler() = default;

  IGpuSampler(const IGpuSampler&) = delete ("IGpuSampler is non-copyable");
  IGpuSampler& operator=(const IGpuSampler&) =
      delete ("IGpuSampler is non-copyable");

  // Mark the start of a new GPU frame on the given command list. Records
  // a "frame start" timestamp and rotates the internal pool ring.
  GECKO_API virtual void BeginFrame(ICommandList& cmd) noexcept = 0;

  // Mark the end of the current GPU frame. Records a "frame end"
  // timestamp and resolves a frame from FramesInFlight ago, emitting all
  // of its zones into IProfiler with ProfSource::GPU.
  GECKO_API virtual void EndFrame(ICommandList& cmd) noexcept = 0;

  // Open a GPU zone. Pairs with EndZone. Nesting is supported up to a
  // backend-defined depth limit (currently 32).
  GECKO_API virtual void BeginZone(ICommandList& cmd, ::gecko::Label label,
                                   const char* name) noexcept = 0;

  GECKO_API virtual void EndZone(ICommandList& cmd) noexcept = 0;

protected:
  IGpuSampler() = default;
  IGpuSampler(IGpuSampler&&) noexcept = default;
  IGpuSampler& operator=(IGpuSampler&&) noexcept = default;
};

// ── RAII helper ──────────────────────────────────────────────────────────

class GpuProfScope
{
public:
  GpuProfScope(IGpuSampler& sampler, ICommandList& cmd, ::gecko::Label label,
               const char* name) noexcept
      : m_Sampler(&sampler), m_Cmd(&cmd)
  {
    sampler.BeginZone(cmd, label, name);
  }

  ~GpuProfScope() noexcept
  {
    if (m_Sampler && m_Cmd)
      m_Sampler->EndZone(*m_Cmd);
  }

  GpuProfScope(const GpuProfScope&) = delete ("GpuProfScope is non-copyable");
  GpuProfScope& operator=(const GpuProfScope&) =
      delete ("GpuProfScope is non-copyable");

  GpuProfScope(GpuProfScope&&) = delete;
  GpuProfScope& operator=(GpuProfScope&&) = delete;

private:
  IGpuSampler* m_Sampler {nullptr};
  ICommandList* m_Cmd {nullptr};
};

}  // namespace gecko::graphics

// ── Macro helpers ────────────────────────────────────────────────────────

#define GECKO_GPU_PROF_CONCAT_IMPL(a, b) a##b
#define GECKO_GPU_PROF_CONCAT(a, b) GECKO_GPU_PROF_CONCAT_IMPL(a, b)

#if defined(GECKO_PROFILING)

#define GECKO_GPU_PROF_SCOPE(sampler_ref, cmd_ref, label, name)       \
  ::gecko::graphics::GpuProfScope GECKO_GPU_PROF_CONCAT(_g_gpu_prof_, \
                                                        __LINE__)     \
  {                                                                   \
    (sampler_ref), (cmd_ref), (label), name                           \
  }

#else

#define GECKO_GPU_PROF_SCOPE(sampler_ref, cmd_ref, label, name) (void)0

#endif
