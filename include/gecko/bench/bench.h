#pragma once

/// @file
/// `gecko::bench` -- minimal benchmark harness for Gecko.
///
/// Each benchmark binary links `Gecko::Bench` and `Gecko::BenchMain`.
/// Cases register at static-init via `GECKO_BENCH(fn)`:
///
/// @code
///   #include <gecko/bench/bench.h>
///   #include <gecko/core/scope.h>
///
///   static void DrawLines(::gecko::bench::State& s)
///   {
///       MyCtx ctx;          // setup runs once (not timed)
///       ctx.Init();
///
///       for (auto _ : s)    // each loop = one timed iteration
///       {
///           {
///               GECKO_PROFILE_NORMAL_NAMED(::gecko::Label{}, "cpu_record");
///               ctx.RecordWork();
///           }
///           {
///               GECKO_PROFILE_NORMAL_NAMED(::gecko::Label{}, "cmd_submit");
///               ctx.SubmitWork();
///           }
///       }
///   }
///   GECKO_BENCH(DrawLines).Iterations(100).Warmup(5);
/// @endcode
///
/// Sub-timings come from the profiler. ANY scope opened with
/// `GECKO_PROFILE_*` (cpu) or `GECKO_GPU_PROF_SCOPE` (gpu) inside an
/// iteration becomes its own metric in the JSON output. The harness
/// also auto-records `frame_total` per iteration.
///
/// Multi-axis sweeps (Cartesian product):
/// @code
///   static void BvhRotate(::gecko::bench::State& s) {
///       const i64 angle = s.Arg("angle");
///       MyBvh bvh; bvh.SetCameraAngle(angle);
///       for (auto _ : s) bvh.Render();
///   }
///   GECKO_BENCH(BvhRotate).Sweep("angle", {0, 45, 90, 135, 180});
/// @endcode

#include <gecko/core/api.h>
#include <gecko/core/labels.h>
#include <gecko/core/types.h>
#include <initializer_list>

namespace gecko::bench {

class Case;  // opaque, defined in .cpp

/// Unit of a recorded metric. Drives axis labels + auto unit-picking
/// in the report. Profiler-derived zones always emit `Ns`.
enum class Unit : ::gecko::u8
{
  Ns,     ///< Nanoseconds. Report auto-picks ns/us/ms/s for display.
  Hz,     ///< Frequency (e.g. fps, iterations per second).
  Count,  ///< Plain dimensionless count.
};

/// Per-case state passed to bench functions.
class State
{
public:
  struct Iterator
  {
    State* S {nullptr};
    int operator*() const noexcept
    {
      return 0;
    }
    Iterator& operator++() noexcept;
    bool operator!=(const Iterator& other) const noexcept;
  };

  GECKO_API Iterator begin() noexcept;
  GECKO_API Iterator end() noexcept;

  /// Sweep value for the current point. Aborts case if `name` was not
  /// registered via `.Sweep(...)` on the builder.
  [[nodiscard]] GECKO_API ::gecko::i64 Arg(const char* name) const noexcept;

  /// Current measured iteration index in `[0, Iterations())`.
  [[nodiscard]] GECKO_API ::gecko::u32 Index() const noexcept;

  /// Total measured iterations (warmup excluded).
  [[nodiscard]] GECKO_API ::gecko::u32 Iterations() const noexcept;

  /// Abort the case from inside the loop. Remaining iterations are
  /// skipped; no result is recorded for the current point.
  GECKO_API void Abort(const char* reason) noexcept;

  /// True if the harness is in a warmup iteration (samples discarded).
  [[nodiscard]] GECKO_API bool IsWarmup() const noexcept;

  /// Record a non-profiler-derived sample for the current iteration.
  /// Use this for metrics that aren't a time interval (FPS, counts,
  /// throughputs). One call per iteration is the intended usage;
  /// multiple calls in the same iteration overwrite. The harness
  /// auto-emits `frame_total` (ns); switching the HTML report to
  /// Rate mode displays it as Hz, so explicit `Record(..., Unit::Hz)`
  /// is only needed when measuring a sub-rate (not the whole iter).
  GECKO_API void Record(const char* name, double value, Unit unit) noexcept;

  // Internals: harness uses these. User code should not.
  struct Impl;
  Impl* m_Impl {nullptr};
};

using CaseFn = void (*)(State&);

/// Fluent configuration returned by `GECKO_BENCH(fn)`.
class GECKO_API Builder
{
public:
  Builder& Name(const char* name) noexcept;
  Builder& Iterations(::gecko::u32 n) noexcept;
  Builder& Warmup(::gecko::u32 n) noexcept;
  Builder& Description(const char* desc) noexcept;
  /// Add an argument axis. Stack multiple `.Sweep` calls for a
  /// Cartesian product. Body queries values via `s.Arg(name)`.
  Builder& Sweep(const char* name,
                 ::std::initializer_list<::gecko::i64> values) noexcept;
  /// Filter captured profiler zones to only those whose Label matches.
  /// When set, engine-internal zones (Vulkan, runtime, ...) are
  /// dropped from the report. Default: capture all zones.
  Builder& MetricLabel(::gecko::Label label) noexcept;

private:
  friend GECKO_API Builder Register(const char* fnName, CaseFn fn) noexcept;
  explicit Builder(Case* c) noexcept : m_Case(c)
  {}
  Case* m_Case;
};

/// Register a case. Use the `GECKO_BENCH(fn)` macro instead.
GECKO_API Builder Register(const char* fnName, CaseFn fn) noexcept;

/// Harness entry point. Default-linked via `Gecko::BenchMain`.
GECKO_API int Main(int argc, char** argv) noexcept;

}  // namespace gecko::bench

/// Register a bench case at static-init time.
#define GECKO_BENCH(fn)                                           \
  static ::gecko::bench::Builder _gecko_bench_##fn = /* NOLINT */ \
      ::gecko::bench::Register(#fn, &fn)
