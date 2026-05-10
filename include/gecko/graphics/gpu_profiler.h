#pragma once

/// @file
/// `IGpuSampler` interface for GPU-side profiling and the RAII zone
/// helpers used by the `GECKO_GPU_*` macros.

#include "gecko/core/api.h"
#include "gecko/core/labels.h"
#include "gecko/core/ptr.h"
#include "gecko/core/services/profiler.h"
#include "gecko/core/types.h"
#include "gecko/graphics/command_list.h"

namespace gecko::graphics {

// GPU profiler ------------------------------------------------------
//
// Records GPU-side timing for a single render queue. Pair every
// `BeginZone` with an `EndZone` on the same command list; nest as
// desired. Frames must be bracketed with `BeginFrame` / `EndFrame` so
// the sampler can rotate its timestamp pools and resolve completed
// frames into `IProfiler` events.
//
// Threading: a single `IGpuSampler` instance is owned by the thread
// that records the queue's command lists for the frame. Calls into
// the profiler happen from `EndFrame` on that same thread.

/// Configuration for `IGpuSampler` creation.
struct GpuSamplerDesc
{
  u32 MaxZonesPerFrame {64};          ///< Worst-case zones per frame (each zone uses 2 timestamps).
  u32 FramesInFlight {3};             ///< Frames kept in flight; must be >= the device's
                                      ///< `MaxFramesInFlight`.
  u32 GpuThreadId {0xFFFF0001u};      ///< Synthetic thread id used for GPU rows in the trace.
  const char* GpuThreadName {"GPU"};  ///< Display name registered for `GpuThreadId`.
};

/// Records GPU-side timing zones for a single render queue.
class IGpuSampler
{
public:
  virtual ~IGpuSampler() = default;

  IGpuSampler(const IGpuSampler&) = delete;
  IGpuSampler& operator=(const IGpuSampler&) = delete;

  /// Mark the start of a new GPU frame on `cmd`. Records a frame-start
  /// timestamp and rotates the internal pool ring.
  GECKO_API virtual void BeginFrame(ICommandList& cmd) noexcept = 0;

  /// Mark the end of the current GPU frame. Records a frame-end
  /// timestamp and resolves a frame from `FramesInFlight` ago, emitting
  /// all of its zones into `IProfiler` with `ProfSource::GPU`.
  GECKO_API virtual void EndFrame(ICommandList& cmd) noexcept = 0;

  /// Open a GPU zone. Pairs with `EndZone`. Nesting is supported up to
  /// a backend-defined depth limit (currently 32).
  /// @param cmd    Command list the zone records into.
  /// @param label  Module label for the zone.
  /// @param name   Display name (zero-copy literal recommended).
  /// @param level  Verbosity level filter.
  GECKO_API virtual void BeginZone(ICommandList& cmd, ::gecko::Label label, const char* name,
                                   ::gecko::ProfLevel level = ::gecko::ProfLevel::Normal) noexcept = 0;

  /// Close the zone most recently opened by `BeginZone` on `cmd`.
  GECKO_API virtual void EndZone(ICommandList& cmd) noexcept = 0;

  /// Notify the sampler that a command list it instrumented has been
  /// submitted to its queue. Called by the device immediately before
  /// the underlying queue-submit call. The sampler uses the first such
  /// CPU timestamp seen for the current frame as the rebase anchor for
  /// that frame's GPU events, so the resulting GPU zones line up with
  /// the CPU `vkQueueSubmit` (etc.) calls in the trace.
  ///
  /// Default is a no-op so backends that don't need it (or null
  /// implementations) don't have to override.
  GECKO_API virtual void OnSubmit(u64 cpuNowNs) noexcept
  {
    (void)cpuNowNs;
  }

protected:
  IGpuSampler() = default;
  IGpuSampler(IGpuSampler&&) noexcept = default;
  IGpuSampler& operator=(IGpuSampler&&) noexcept = default;
};

// RAII helper -------------------------------------------------------

/// RAII guard that wraps a `BeginZone` / `EndZone` pair around its
/// lifetime. Use the `GECKO_GPU_PROF_SCOPE` macro to declare one.
class [[nodiscard("GpuProfScope is a RAII guard - name the variable, e.g. via "
                  "GECKO_GPU_PROF_SCOPE")]] GpuProfScope
{
public:
  GpuProfScope(IGpuSampler& sampler, ICommandList& cmd, ::gecko::Label label, const char* name,
               ::gecko::ProfLevel level = ::gecko::ProfLevel::Normal) noexcept
      : m_Sampler(&sampler), m_Cmd(&cmd)
  {
    sampler.BeginZone(cmd, label, name, level);
  }

  ~GpuProfScope() noexcept
  {
    if (m_Sampler && m_Cmd)
      m_Sampler->EndZone(*m_Cmd);
  }

  GpuProfScope(const GpuProfScope&) = delete;
  GpuProfScope& operator=(const GpuProfScope&) = delete;

  GpuProfScope(GpuProfScope&&) = delete;
  GpuProfScope& operator=(GpuProfScope&&) = delete;

private:
  IGpuSampler* m_Sampler {nullptr};
  ICommandList* m_Cmd {nullptr};
};

/// RAII guard that picks up the `IGpuSampler` from the command list
/// (attached via `AttachGpuSampler`). Silently no-ops if the command
/// list has no attached sampler.
class [[nodiscard("GpuAutoProfScope is a RAII guard - name the variable, e.g. via "
                  "GECKO_GPU_SCOPE_*")]] GpuAutoProfScope
{
public:
  GpuAutoProfScope(ICommandList& cmd, ::gecko::Label label, const char* name, ::gecko::ProfLevel level) noexcept
      : m_Sampler(cmd.GetAttachedGpuSampler()), m_Cmd(&cmd)
  {
    if (m_Sampler)
      m_Sampler->BeginZone(cmd, label, name, level);
  }

  ~GpuAutoProfScope() noexcept
  {
    if (m_Sampler && m_Cmd)
      m_Sampler->EndZone(*m_Cmd);
  }

  GpuAutoProfScope(const GpuAutoProfScope&) = delete;
  GpuAutoProfScope& operator=(const GpuAutoProfScope&) = delete;
  GpuAutoProfScope(GpuAutoProfScope&&) = delete;
  GpuAutoProfScope& operator=(GpuAutoProfScope&&) = delete;

private:
  IGpuSampler* m_Sampler {nullptr};
  ICommandList* m_Cmd {nullptr};
};

}  // namespace gecko::graphics

// -- Macro helpers --------------------------------------------------------

#define GECKO_GPU_PROF_CONCAT_IMPL(a, b) a##b
#define GECKO_GPU_PROF_CONCAT(a, b) GECKO_GPU_PROF_CONCAT_IMPL(a, b)

#if defined(GECKO_PROFILING)

#define GECKO_GPU_PROF_SCOPE(sampler_ref, cmd_ref, label, name)                 \
  ::gecko::graphics::GpuProfScope GECKO_GPU_PROF_CONCAT(_g_gpu_prof_, __LINE__) \
  {                                                                             \
    (sampler_ref), (cmd_ref), (label), name                                     \
  }

// -- Ergonomic GPU scope macros (mirror CPU GECKO_SCOPE_*) ---------
//
// Sampler is picked up from the command list via AttachGpuSampler; pass
// the cmd, a Label, and a literal name. Compiles out at GECKO_PROFILING=0.
//
// Levels:
//   _NAMED         -> Detailed (default for renderer fine-grain zones)
//   _NORMAL_NAMED  -> Normal
//   _ALWAYS_NAMED  -> Always

#define GECKO_GPU_SCOPE_NAMED(cmd_ref, label, name)                                 \
  ::gecko::graphics::GpuAutoProfScope GECKO_GPU_PROF_CONCAT(_g_gpu_auto_, __LINE__) \
  {                                                                                 \
    (cmd_ref), (label), name, ::gecko::ProfLevel::Detailed                          \
  }

#define GECKO_GPU_SCOPE_NORMAL_NAMED(cmd_ref, label, name)                          \
  ::gecko::graphics::GpuAutoProfScope GECKO_GPU_PROF_CONCAT(_g_gpu_auto_, __LINE__) \
  {                                                                                 \
    (cmd_ref), (label), name, ::gecko::ProfLevel::Normal                            \
  }

#define GECKO_GPU_SCOPE_ALWAYS_NAMED(cmd_ref, label, name)                          \
  ::gecko::graphics::GpuAutoProfScope GECKO_GPU_PROF_CONCAT(_g_gpu_auto_, __LINE__) \
  {                                                                                 \
    (cmd_ref), (label), name, ::gecko::ProfLevel::Always                            \
  }

#else

#define GECKO_GPU_PROF_SCOPE(sampler_ref, cmd_ref, label, name) (void)0
#define GECKO_GPU_SCOPE_NAMED(cmd_ref, label, name) (void)0
#define GECKO_GPU_SCOPE_NORMAL_NAMED(cmd_ref, label, name) (void)0
#define GECKO_GPU_SCOPE_ALWAYS_NAMED(cmd_ref, label, name) (void)0

#endif
