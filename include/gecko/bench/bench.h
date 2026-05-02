#pragma once

/// @file
/// `gecko::bench` -- minimal benchmark harness for Gecko.
///
/// Each benchmark binary links `Gecko::Bench` (the harness lib) and
/// `Gecko::BenchMain` (which provides `main()`). Cases are registered
/// at static-init time via `GECKO_BENCH(fn)`:
///
/// @code
///   #include <gecko/bench/bench.h>
///
///   static void DebugLines(::gecko::bench::State& s)
///   {
///       // Setup runs once (excluded from timing).
///       MyContext ctx;
///       ctx.Init();
///
///       for (auto _ : s)              // Each iteration is timed.
///       {
///           ctx.DoWork();
///       }
///       // Teardown runs once after the loop.
///   }
///   GECKO_BENCH(DebugLines)
///       .Iterations(100)
///       .Warmup(5);
/// @endcode
///
/// Multi-axis sweeps:
/// @code
///   static void ManyLines(::gecko::bench::State& s)
///   {
///       const ::gecko::i64 count = s.Arg("count");
///       for (auto _ : s) { /* draw `count` lines */ }
///   }
///   GECKO_BENCH(ManyLines).Sweep("count", {100, 1000, 10000, 100000});
/// @endcode

#include <gecko/core/api.h>
#include <gecko/core/types.h>
#include <initializer_list>

namespace gecko::bench {

/// One sub-timing slice recorded inside an iteration via
/// `ScopedSection` (e.g. "cmd_record", "draw_dispatch"). Aggregated
/// per-section across iterations and reported alongside the total.
struct Section
{
  const char* Name {nullptr};
  ::gecko::u64 TotalNs {0};
  ::gecko::u32 Count {0};
};

/// Per-case state object passed to bench functions. Driven by the
/// harness; not constructible by users.
class State
{
public:
  /// Range-for support: `for (auto _ : s) { ... }` iterates one
  /// timed iteration per loop body, auto-tracking start/stop.
  struct Iterator
  {
    State* S {nullptr};
    int operator*() const noexcept
    {
      return 0;
    }
    Iterator& operator++() noexcept
    {
      S->NextIter();
      return *this;
    }
    bool operator!=(const Iterator& other) const noexcept
    {
      return S != nullptr && S->m_Index < S->m_Total && !S->m_Aborted &&
             other.S == nullptr;
    }
  };

  GECKO_API Iterator begin() noexcept;
  GECKO_API Iterator end() noexcept;

  /// Lookup a sweep argument by name. Aborts the case with a warning
  /// if the name was not registered via `.Sweep(...)`.
  [[nodiscard]] GECKO_API ::gecko::i64 Arg(const char* name) const noexcept;

  /// Iteration index in `[0, Iterations())` (only meaningful inside
  /// the loop body).
  [[nodiscard]] ::gecko::u32 Index() const noexcept
  {
    return m_Index;
  }

  /// Total iterations the harness is going to run for this case
  /// (warmup excluded).
  [[nodiscard]] ::gecko::u32 Iterations() const noexcept
  {
    return m_Total;
  }

  /// Abort the case from inside the loop. Remaining iterations are
  /// skipped; no result is recorded.
  GECKO_API void Abort(const char* reason) noexcept;

  /// Record a custom counter value sampled at the current iteration
  /// (e.g. lines submitted, MB uploaded). Reported in the JSON.
  GECKO_API void Counter(const char* name, ::gecko::i64 value) noexcept;

  /// RAII helper: time a sub-section of the iteration. The section
  /// total is reported as a separate stat. Does not stop or replace
  /// the per-iteration timer.
  class GECKO_API ScopedSection
  {
  public:
    ScopedSection(State& s, const char* name) noexcept;
    ~ScopedSection() noexcept;
    ScopedSection(const ScopedSection&) = delete;
    ScopedSection& operator=(const ScopedSection&) = delete;

  private:
    State& m_State;
    const char* m_Name;
    ::gecko::u64 m_StartNs;
  };

  // Internal -- driven by the harness. Stable across translation units.
  GECKO_API void NextIter() noexcept;
  GECKO_API void StartFirstIter() noexcept;

  // Internals exposed through opaque pimpl in bench.cpp; keep struct
  // POD-light here so user code can pass `State&` cheaply.
  struct Impl;
  Impl* m_Impl {nullptr};
  ::gecko::u32 m_Index {0};
  ::gecko::u32 m_Total {0};
  ::gecko::u32 m_Warmup {0};
  bool m_Started {false};
  bool m_Aborted {false};
  ::gecko::u64 m_IterStartNs {0};
};

/// Function signature of a bench case.
using CaseFn = void (*)(State&);

/// Builder returned by `GECKO_BENCH(fn)` for fluent configuration.
/// All setters return `*this` so they can be chained.
class GECKO_API Builder
{
public:
  /// Override the case's display name (default: stringified function
  /// name). Useful when a bench function is reused across cases.
  Builder& Name(const char* name) noexcept;

  /// Set the number of measured iterations. Default: 100.
  Builder& Iterations(::gecko::u32 n) noexcept;

  /// Set the number of warmup iterations (not measured). Default: 5.
  Builder& Warmup(::gecko::u32 n) noexcept;

  /// Free-text description recorded in the JSON output.
  Builder& Description(const char* desc) noexcept;

  /// Add an argument axis: the case is run once per value, and the
  /// case body queries the value via `s.Arg(name)`. Stack multiple
  /// `.Sweep(...)` calls for a Cartesian product.
  Builder& Sweep(const char* name,
                 ::std::initializer_list<::gecko::i64> values) noexcept;

private:
  friend GECKO_API Builder Register(const char* fnName, CaseFn fn) noexcept;
  explicit Builder(class Case* c) noexcept : m_Case(c)
  {}
  class Case* m_Case;
};

/// Register a case with the harness. Returns a `Builder` for chaining.
/// Use the `GECKO_BENCH(fn)` macro instead of calling this directly.
GECKO_API Builder Register(const char* fnName, CaseFn fn) noexcept;

/// Harness entry point. Default-linked via `Gecko::BenchMain`; bench
/// binaries can call this from a custom `main()` if they need
/// pre-`main` work that the linkable main() doesn't allow.
GECKO_API int Main(int argc, char** argv) noexcept;

}  // namespace gecko::bench

/// Register a bench case. The function name doubles as the case's
/// display name; override with `.Name("display_name")` on the builder.
#define GECKO_BENCH(fn)                                           \
  static ::gecko::bench::Builder _gecko_bench_##fn = /* NOLINT */ \
      ::gecko::bench::Register(#fn, &fn)
