#pragma once

/// @file
/// Lightweight in-process profiler service.
///
/// Use `GECKO_PROFILE` / `GECKO_PROFILE_NORMAL` / `GECKO_PROFILE_ALWAYS`
/// (or the combined `GECKO_SCOPE*` macros from `<gecko/core/scope.h>`)
/// to time scopes; `GECKO_COUNTER` to record a value; `GECKO_FRAME`
/// to mark frame boundaries. Sinks (Chrome trace writer, ETW, ...)
/// register through `IProfilerSink::RegisterWith(profiler)`.

#include "gecko/core/api.h"
#include "gecko/core/labels.h"
#include "gecko/core/sink_registration.h"
#include "gecko/core/span.h"
#include "gecko/core/types.h"

namespace gecko {

/// Verbosity tier of a profiler event. Compile-time gated by
/// `GECKO_PROF_MAX_LEVEL` and runtime gated by `IProfiler::SetMinLevel`.
enum class ProfLevel : u8
{
  Always = 0,    ///< Frame markers, errors, never compiled out.
  Normal = 1,    ///< Default for game-loop work.
  Detailed = 2,  ///< Inner-loop detail; off in release by default.
};

// Compile-time max level (set via CMake per-module)
#ifndef GECKO_PROF_MAX_LEVEL
#ifdef NDEBUG
#define GECKO_PROF_MAX_LEVEL 1  // Release: Always + Normal
#else
#define GECKO_PROF_MAX_LEVEL 2  // Debug: Everything
#endif
#endif

#define GECKO_PROF_LEVEL_ALWAYS 0
#define GECKO_PROF_LEVEL_NORMAL 1
#define GECKO_PROF_LEVEL_DETAILED 2

/// Type of an event written to the profiler stream.
enum class ProfEventKind : u8
{
  ZoneBegin,  ///< Start of a timed scope.
  ZoneEnd,    ///< End of a timed scope; pairs with ZoneBegin.
  Counter,    ///< Single sampled value; emitted by `GECKO_COUNTER`.
  FrameMark   ///< Frame boundary; emitted by `GECKO_FRAME`.
};

/// Origin domain of a profiler event.
enum class ProfSource : u8
{
  CPU = 0,  ///< Sampled on the host CPU.
  GPU = 1,  ///< Sampled on the GPU (resolved via timestamp queries).
};

// Category 0 means "uncategorized". Modules call IProfiler::RegisterCategory()
// to obtain a stable u8 id (1..63) and pass it via GECKO_PROF_SCOPE_CAT.
/// One profiler record. Padded to a full cache line so producers may
/// hand records to a SPSC ring without false sharing.
struct alignas(64) ProfEvent
{
  u64 TimestampNs {0};         ///< Capture time, ns since boot.
  u64 Value {0};               ///< Counter value (Kind==Counter).
  const char* Name {nullptr};  ///< Static name pointer.
  Label EventLabel {};         ///< Originating scope label.
  u32 ThreadId {0};            ///< Logical thread id.
  u32 NameHash {0};            ///< FNV-1a of `Name`.
  ProfEventKind Kind {ProfEventKind::ZoneBegin};
  ProfLevel Level {ProfLevel::Normal};
  ProfSource Source {ProfSource::CPU};
  u8 Category {0};  ///< 0=uncategorised; 1..63 from `RegisterCategory`.
};

static_assert(sizeof(ProfEvent) == 64, "ProfEvent must be 64 bytes");

struct IProfiler;

/// Sink that consumes profiler events. Implementations are e.g. the
/// Chrome-trace writer or an ETW emitter. Registered through
/// `RegisteredSink::RegisterWith(profiler)`.
struct IProfilerSink : public RegisteredSink<IProfilerSink, IProfiler>
{
  virtual ~IProfilerSink() = default;
  /// Receive a single event.
  virtual void Write(const ProfEvent& event) noexcept = 0;
  /// Receive a batch of events. Implementations may forward each entry
  /// to `Write` if they have no batching path.
  virtual void WriteBatch(Span<const ProfEvent> events) noexcept = 0;
  /// Block until queued events have been written out.
  virtual void Flush() noexcept = 0;
};

// Per-scope aggregator snapshot. Updated on ZoneEnd for *every* level
// (cheap: one CAS per zone-end). The windowed counters (MinNs, MaxNs,
// Count) reset on a configurable timer (default 1 s) or via
// IProfiler::ResetStats(). LastNs and the per-WatchScope rolling ring are
// intentionally NOT cleared by the auto-reset - they represent "the most
// recent observation" and "the last N actual samples" respectively, so
// HUD readers never see 0 ms in the gap between reset and the next
// ZoneEnd. AvgNs is non-zero only for scopes registered via WatchScope() -
// those get a fixed-window rolling average over the last N samples.
struct ScopeStats
{
  u64 LastNs {0};        ///< Most recent sample (never auto-cleared).
  u64 MinNs {~u64 {0}};  ///< Min over the current window.
  u64 MaxNs {0};         ///< Max over the current window.
  u64 AvgNs {0};         ///< 0 unless scope is registered via `WatchScope`.
  u32 Count {0};         ///< Samples seen in the current window.
};

/// Counters returned by `IProfiler::GetDiagnostics`.
struct ProfilerDiagnostics
{
  u64 DroppedEvents {0};       ///< Events dropped because the ring was full.
  u64 ReentrantDrops {0};      ///< Events dropped due to reentrant emit guards.
  u64 AggregatorOverflow {0};  ///< Scope-aggregator slot exhaustion.
};

/// Maximum number of categories an `IProfiler` will track.
constexpr u8 ProfMaxCategories = 64;
/// Sentinel returned by `RegisterCategory` / `FindCategory` on failure.
constexpr u8 ProfInvalidCategory = 0xFF;

struct IProfilerStream;  // forward decl for DumpStats

/// Profiler service interface. Apps usually go through the macros.
struct IProfiler
{
  virtual ~IProfiler() = default;
  virtual void Emit(const ProfEvent& ev) noexcept = 0;
  virtual u64 NowNs() const noexcept = 0;
  virtual void SetMinLevel(ProfLevel level) noexcept = 0;
  virtual ProfLevel GetMinLevel() const noexcept = 0;
  virtual bool IsLevelEnabled(ProfLevel level) const noexcept = 0;
  virtual void AddSink(IProfilerSink* sink) noexcept = 0;
  virtual void RemoveSink(IProfilerSink* sink) noexcept = 0;
  virtual bool Init() noexcept = 0;
  virtual void Shutdown() noexcept = 0;

  // Trace output gate. When false, sinks receive nothing (file/network
  // writes pause), but the in-process aggregator keeps tracking -- query
  // GetStats() to read live timings. Default: true.
  virtual void SetTraceEnabled(bool enabled) noexcept = 0;
  virtual bool IsTraceEnabled() const noexcept = 0;

  // Detailed-level sample rate. N=1: every Detailed event emitted (default).
  // N=K (K>1): emit only every K-th Detailed event per scope. N=0: emit
  // none. Use to keep trace files manageable when vsync is off / hot loops
  // dominate. Normal/Always are unaffected.
  virtual void SetDetailedSampleRate(u32 nthEvent) noexcept = 0;
  virtual u32 GetDetailedSampleRate() const noexcept = 0;

  // Aggregator query. Returns a snapshot of stats for the given scope.
  // Returns a default-constructed ScopeStats if unseen since the last reset.
  // The (const char*) overload hashes once per call -- cache the hash
  // yourself if you query in a hot loop.
  virtual ScopeStats GetStats(u32 nameHash, ProfSource source = ProfSource::CPU) const noexcept = 0;
  ScopeStats GetStats(const char* name, ProfSource source = ProfSource::CPU) const noexcept;

  // Watch a scope for rolling-average tracking. AvgNs in the returned
  // ScopeStats becomes the mean of the last `windowSize` samples. Calling
  // again with a different window resizes. Use sparingly -- each watched
  // scope owns a ring of `windowSize * 8` bytes.
  virtual void WatchScope(u32 nameHash, u32 windowSize = 256, ProfSource source = ProfSource::CPU) noexcept = 0;
  void WatchScope(const char* name, u32 windowSize = 256, ProfSource source = ProfSource::CPU) noexcept;
  virtual void UnwatchScope(u32 nameHash, ProfSource source = ProfSource::CPU) noexcept = 0;
  void UnwatchScope(const char* name, ProfSource source = ProfSource::CPU) noexcept;

  // Reset all aggregator stats. Called automatically on a configurable
  // timer (see SetStatsResetIntervalMs). Set interval to 0 to disable
  // auto-reset (useful for one-shot tools / non-game-loop apps).
  virtual void ResetStats() noexcept = 0;
  virtual void SetStatsResetIntervalMs(u32 ms) noexcept = 0;
  virtual u32 GetStatsResetIntervalMs() const noexcept = 0;

  // Iterate every scope currently in the aggregator. Callback shape:
  //   void(*)(const char* name, u32 nameHash, ProfSource source,
  //           const ScopeStats& stats, void* user)
  using ForEachScopeFn = void (*)(const char* name, u32 nameHash, ProfSource source, const ScopeStats& stats,
                                  void* user);
  virtual void ForEachScope(ForEachScopeFn fn, void* user) const noexcept = 0;

  // Print a human-readable table of every scope to GECKO_INFO under the
  // given label. Intended for an F-key debug dump.
  virtual void DumpStats(Label label) const noexcept = 0;

  // Categories. RegisterCategory returns a stable id (1..63) for the given
  // name; subsequent calls with the same name return the same id. Returns
  // ProfInvalidCategory on overflow or null name. The implementation copies
  // the name into owned storage, so callers may pass non-static buffers.
  virtual u8 RegisterCategory(const char* name) noexcept = 0;
  virtual void SetCategoryEnabled(u8 id, bool on) noexcept = 0;
  virtual bool IsCategoryEnabled(u8 id) const noexcept = 0;
  virtual u8 FindCategory(const char* name) const noexcept = 0;
  virtual const char* GetCategoryName(u8 id) const noexcept = 0;

  virtual ProfilerDiagnostics GetDiagnostics() const noexcept = 0;
};

GECKO_API IProfiler* GetProfiler() noexcept;
GECKO_API u32 ThisThreadId() noexcept;

// Optional human-readable name for this thread. Stored in TLS; sinks emit
// a Chrome-trace 'thread_name' metadata record on first sight per thread.
GECKO_API void SetThreadProfilerName(const char* name) noexcept;
GECKO_API const char* GetThreadProfilerName() noexcept;

// Cross-thread lookup: returns the name registered for the given TID by
// SetThreadProfilerName (any thread), or nullptr. Used by sinks emitting
// thread_name metadata.
GECKO_API const char* LookupThreadProfilerName(u32 threadId) noexcept;

// Register a profiler-side name for a synthetic / virtual thread id. Unlike
// SetThreadProfilerName this does NOT touch TLS -- use it for non-OS-thread
// rows in the trace (e.g. GPU queues, async I/O lanes). Pass nullptr to
// remove a previously registered name.
GECKO_API void RegisterThreadProfilerName(u32 threadId, const char* name) noexcept;

struct [[nodiscard("ProfScope is a RAII guard - name the variable, e.g. via GECKO_PROFILE")]] ProfScope
{
  Label ScopeLabel {};                  // 16 bytes
  u64 Time0 {0};                        // 8 bytes
  const char* Name {nullptr};           // 8 bytes (allows custom name vs label)
  u32 NameHash {0};                     // 4 bytes
  u32 ThreadId {0};                     // 4 bytes
  ProfLevel Level {ProfLevel::Normal};  // 1 byte
  u8 Category {0};                      // 1 byte
  bool Enabled {false};                 // 1 byte
  // 5 bytes padding -> 48 bytes total

  ProfScope(Label label, u32 hash, const char* name, ProfLevel lvl, u8 cat = 0) noexcept;
  ~ProfScope() noexcept;
  ProfScope(const ProfScope&) = delete;
  ProfScope& operator=(const ProfScope&) = delete;
};

}  // namespace gecko

#ifndef GECKO_PROFILING
#define GECKO_PROFILING 1
#endif

#if GECKO_PROFILING

#define GECKO_PROF_CONCAT_(x, y) x##y
#define GECKO_PROF_CONCAT(x, y) GECKO_PROF_CONCAT_(x, y)

// ------------------------------------------------------------
// Profiler-only scope macros. These do NOT push a memory label.
// Consumers should usually prefer the combined macros in <gecko/core/scope.h>
// (`GECKO_SCOPE`, `GECKO_SCOPE_NAMED`, ...) which push a label AND start a
// profiler scope. The macros below are provided for engine-internal sites
// that explicitly want a profiler scope without affecting the label stack.
//
//   GECKO_PROFILE                -> Detailed, name = __func__
//   GECKO_PROFILE_NAMED          -> Detailed, custom name literal
//   GECKO_PROFILE_CAT            -> Detailed + category
//   GECKO_PROFILE_NORMAL[_NAMED|_CAT]
//   GECKO_PROFILE_ALWAYS[_NAMED|_CAT]
// ------------------------------------------------------------

#if GECKO_PROF_MAX_LEVEL >= GECKO_PROF_LEVEL_DETAILED
#define GECKO_PROFILE(label)                                                  \
  ::gecko::ProfScope GECKO_PROF_CONCAT(_g_prof_, __LINE__)                    \
  {                                                                           \
    (label), ::gecko::FNV1a(__func__), __func__, ::gecko::ProfLevel::Detailed \
  }
#define GECKO_PROFILE_NAMED(label, name)                                     \
  ::gecko::ProfScope GECKO_PROF_CONCAT(_g_prof_, __LINE__)                   \
  {                                                                          \
    (label), ::gecko::FNV1aLiteral(name), name, ::gecko::ProfLevel::Detailed \
  }
#define GECKO_PROFILE_CAT(label, name, cat)                                                      \
  ::gecko::ProfScope GECKO_PROF_CONCAT(_g_prof_, __LINE__)                                       \
  {                                                                                              \
    (label), ::gecko::FNV1aLiteral(name), name, ::gecko::ProfLevel::Detailed, (::gecko::u8)(cat) \
  }
#else
#define GECKO_PROFILE(label) (void)0
#define GECKO_PROFILE_NAMED(label, name) (void)0
#define GECKO_PROFILE_CAT(label, name, cat) (void)0
#endif

#if GECKO_PROF_MAX_LEVEL >= GECKO_PROF_LEVEL_NORMAL
#define GECKO_PROFILE_NORMAL(label)                                         \
  ::gecko::ProfScope GECKO_PROF_CONCAT(_g_prof_, __LINE__)                  \
  {                                                                         \
    (label), ::gecko::FNV1a(__func__), __func__, ::gecko::ProfLevel::Normal \
  }
#define GECKO_PROFILE_NORMAL_NAMED(label, name)                            \
  ::gecko::ProfScope GECKO_PROF_CONCAT(_g_prof_, __LINE__)                 \
  {                                                                        \
    (label), ::gecko::FNV1aLiteral(name), name, ::gecko::ProfLevel::Normal \
  }
#define GECKO_PROFILE_NORMAL_CAT(label, name, cat)                                             \
  ::gecko::ProfScope GECKO_PROF_CONCAT(_g_prof_, __LINE__)                                     \
  {                                                                                            \
    (label), ::gecko::FNV1aLiteral(name), name, ::gecko::ProfLevel::Normal, (::gecko::u8)(cat) \
  }
#else
#define GECKO_PROFILE_NORMAL(label) (void)0
#define GECKO_PROFILE_NORMAL_NAMED(label, name) (void)0
#define GECKO_PROFILE_NORMAL_CAT(label, name, cat) (void)0
#endif

#define GECKO_PROFILE_ALWAYS(label)                                         \
  ::gecko::ProfScope GECKO_PROF_CONCAT(_g_prof_, __LINE__)                  \
  {                                                                         \
    (label), ::gecko::FNV1a(__func__), __func__, ::gecko::ProfLevel::Always \
  }
#define GECKO_PROFILE_ALWAYS_NAMED(label, name)                            \
  ::gecko::ProfScope GECKO_PROF_CONCAT(_g_prof_, __LINE__)                 \
  {                                                                        \
    (label), ::gecko::FNV1aLiteral(name), name, ::gecko::ProfLevel::Always \
  }
#define GECKO_PROFILE_ALWAYS_CAT(label, name, cat)                                             \
  ::gecko::ProfScope GECKO_PROF_CONCAT(_g_prof_, __LINE__)                                     \
  {                                                                                            \
    (label), ::gecko::FNV1aLiteral(name), name, ::gecko::ProfLevel::Always, (::gecko::u8)(cat) \
  }

#define GECKO_COUNTER(label, name, val)                                \
  do                                                                   \
  {                                                                    \
    if (auto* p = ::gecko::GetProfiler())                              \
    {                                                                  \
      ::gecko::ProfEvent ev {.TimestampNs = p->NowNs(),                \
                             .Value = (::gecko::u64)(val),             \
                             .Name = name,                             \
                             .EventLabel = (label),                    \
                             .ThreadId = ::gecko::ThisThreadId(),      \
                             .NameHash = ::gecko::FNV1aLiteral(name),  \
                             .Kind = ::gecko::ProfEventKind::Counter}; \
      p->Emit(ev);                                                     \
    }                                                                  \
  } while (0)

#define GECKO_FRAME(label, name)                                         \
  do                                                                     \
  {                                                                      \
    if (auto* p = ::gecko::GetProfiler())                                \
    {                                                                    \
      ::gecko::ProfEvent ev {.TimestampNs = p->NowNs(),                  \
                             .Name = name,                               \
                             .EventLabel = (label),                      \
                             .ThreadId = ::gecko::ThisThreadId(),        \
                             .NameHash = ::gecko::FNV1aLiteral(name),    \
                             .Kind = ::gecko::ProfEventKind::FrameMark}; \
      p->Emit(ev);                                                       \
    }                                                                    \
  } while (0)

#else  // !GECKO_PROFILING

#define GECKO_PROFILE(label) (void)0
#define GECKO_PROFILE_NAMED(label, name) (void)0
#define GECKO_PROFILE_CAT(label, name, cat) (void)0
#define GECKO_PROFILE_NORMAL(label) (void)0
#define GECKO_PROFILE_NORMAL_NAMED(label, name) (void)0
#define GECKO_PROFILE_NORMAL_CAT(label, name, cat) (void)0
#define GECKO_PROFILE_ALWAYS(label) (void)0
#define GECKO_PROFILE_ALWAYS_NAMED(label, name) (void)0
#define GECKO_PROFILE_ALWAYS_CAT(label, name, cat) (void)0
#define GECKO_COUNTER(label, name, val) (void)0
#define GECKO_FRAME(label, name) (void)0

#endif  // GECKO_PROFILING

namespace gecko {

inline ProfScope::ProfScope(Label label, u32 hash, const char* name, ProfLevel lvl, u8 cat) noexcept
    : ScopeLabel(label), Name(name), NameHash(hash), ThreadId(ThisThreadId()), Level(lvl), Category(cat)
{
  if (auto* prof = GetProfiler(); prof && prof->IsLevelEnabled(Level) && prof->IsCategoryEnabled(Category))
  {
    Enabled = true;
    Time0 = prof->NowNs();
    prof->Emit({.TimestampNs = Time0,
                .Name = Name,
                .EventLabel = ScopeLabel,
                .ThreadId = ThreadId,
                .NameHash = NameHash,
                .Kind = ProfEventKind::ZoneBegin,
                .Level = Level,
                .Source = ProfSource::CPU,
                .Category = Category});
  }
}

inline ProfScope::~ProfScope() noexcept
{
  if (Enabled)
    if (auto* prof = GetProfiler())
      prof->Emit({.TimestampNs = prof->NowNs(),
                  .Name = Name,
                  .EventLabel = ScopeLabel,
                  .ThreadId = ThreadId,
                  .NameHash = NameHash,
                  .Kind = ProfEventKind::ZoneEnd,
                  .Level = Level,
                  .Source = ProfSource::CPU,
                  .Category = Category});
}

// NullProfiler - default no-op
struct NullProfiler final : IProfiler
{
  ProfLevel m_MinLevel {ProfLevel::Normal};

  void Emit(const ProfEvent&) noexcept override
  {}
  u64 NowNs() const noexcept override
  {
    return 0;
  }
  void SetMinLevel(ProfLevel level) noexcept override
  {
    m_MinLevel = level;
  }
  ProfLevel GetMinLevel() const noexcept override
  {
    return m_MinLevel;
  }
  bool IsLevelEnabled(ProfLevel level) const noexcept override
  {
    return level <= m_MinLevel;
  }
  void AddSink(IProfilerSink*) noexcept override
  {}
  void RemoveSink(IProfilerSink*) noexcept override
  {}
  bool Init() noexcept override
  {
    return true;
  }
  void Shutdown() noexcept override
  {}
  void SetTraceEnabled(bool) noexcept override
  {}
  bool IsTraceEnabled() const noexcept override
  {
    return false;
  }
  void SetDetailedSampleRate(u32) noexcept override
  {}
  u32 GetDetailedSampleRate() const noexcept override
  {
    return 1;
  }
  ScopeStats GetStats(u32, ProfSource) const noexcept override
  {
    return {};
  }
  void WatchScope(u32, u32, ProfSource) noexcept override
  {}
  void UnwatchScope(u32, ProfSource) noexcept override
  {}
  void ResetStats() noexcept override
  {}
  void SetStatsResetIntervalMs(u32) noexcept override
  {}
  u32 GetStatsResetIntervalMs() const noexcept override
  {
    return 0;
  }
  void ForEachScope(ForEachScopeFn, void*) const noexcept override
  {}
  void DumpStats(Label) const noexcept override
  {}
  u8 RegisterCategory(const char*) noexcept override
  {
    return 0;
  }
  void SetCategoryEnabled(u8, bool) noexcept override
  {}
  bool IsCategoryEnabled(u8) const noexcept override
  {
    return true;
  }
  u8 FindCategory(const char*) const noexcept override
  {
    return ProfInvalidCategory;
  }
  const char* GetCategoryName(u8) const noexcept override
  {
    return nullptr;
  }
  ProfilerDiagnostics GetDiagnostics() const noexcept override
  {
    return {};
  }
};

// String-overload helpers (inline; hash on the fly).
inline ScopeStats IProfiler::GetStats(const char* name, ProfSource source) const noexcept
{
  return GetStats(name ? FNV1a(name) : 0u, source);
}
inline void IProfiler::WatchScope(const char* name, u32 windowSize, ProfSource source) noexcept
{
  if (name)
    WatchScope(FNV1a(name), windowSize, source);
}
inline void IProfiler::UnwatchScope(const char* name, ProfSource source) noexcept
{
  if (name)
    UnwatchScope(FNV1a(name), source);
}

}  // namespace gecko
