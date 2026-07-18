#pragma once

#include "gecko/core/api.h"
#include "gecko/core/labels.h"
#include "gecko/core/types.h"
#include "gecko/core/utility/hash.h"

namespace gecko {

enum class ProfLevel : u8
{
  Always,
  Normal,
  Detailed,
};

#ifndef GECKO_PROF_MAX_LEVEL
#ifdef NDEBUG
#define GECKO_PROF_MAX_LEVEL 1
#else
#define GECKO_PROF_MAX_LEVEL 2
#endif
#endif

#define GECKO_PROF_LEVEL_ALWAYS 0
#define GECKO_PROF_LEVEL_NORMAL 1
#define GECKO_PROF_LEVEL_DETAILED 2

enum class ProfEventKind : u8
{
  ZoneBegin,
  ZoneEnd,
  Counter,
  FrameMark,
};

enum class ProfSource : u8
{
  CPU,
  GPU,
};

struct alignas(64) ProfEvent
{
  u64 TimestampNs {0};
  u64 Value {0};
  const char* Name {nullptr};
  Label EventLabel {};
  u32 ThreadId {0};
  u32 NameHash {0};
  ProfEventKind Kind {ProfEventKind::ZoneBegin};
  ProfLevel Level {ProfLevel::Normal};
  ProfSource Source {ProfSource::CPU};
  u8 Category {0};
};

static_assert(sizeof(ProfEvent) == 64);

struct ScopeStats
{
  u64 LastNs {0};
  u64 MinNs {U64Max};
  u64 MaxNs {0};
  u64 AverageNs {0};
  u64 TotalNs {0};
  u32 Count {0};
};

struct ProfilerDiagnostics
{
  u64 DroppedEvents {0};
  u64 OpenZoneOverflow {0};
  u64 StatsOverflow {0};
};

using ForEachScopeFn = void (*)(const char* name, u32 nameHash, ProfSource source, const ScopeStats& stats, void* user);

GECKO_API void EmitProfileEvent(const ProfEvent& event) noexcept;
[[nodiscard]] GECKO_API u64 ProfilerNowNs() noexcept;
GECKO_API void SetProfilerLevel(ProfLevel level) noexcept;
[[nodiscard]] GECKO_API ProfLevel GetProfilerLevel() noexcept;
[[nodiscard]] GECKO_API bool IsProfilerLevelEnabled(ProfLevel level) noexcept;
[[nodiscard]] GECKO_API ScopeStats GetScopeStats(u32 nameHash, ProfSource source = ProfSource::CPU) noexcept;
[[nodiscard]] inline ScopeStats GetScopeStats(const char* name, ProfSource source = ProfSource::CPU) noexcept
{
  return GetScopeStats(name != nullptr ? FNV1a(name) : 0, source);
}
GECKO_API void ResetProfilerStats() noexcept;
GECKO_API void ForEachProfileScope(ForEachScopeFn callback, void* user) noexcept;
GECKO_API void DumpProfilerStats(Label label) noexcept;
[[nodiscard]] GECKO_API ProfilerDiagnostics GetProfilerDiagnostics() noexcept;

GECKO_API u32 ThisThreadId() noexcept;
GECKO_API void SetThreadProfilerName(const char* name) noexcept;
[[nodiscard]] GECKO_API const char* GetThreadProfilerName() noexcept;
[[nodiscard]] GECKO_API const char* LookupThreadProfilerName(u32 threadId) noexcept;
GECKO_API void RegisterThreadProfilerName(u32 threadId, const char* name) noexcept;

struct [[nodiscard("Name profiler scope variables; prefer GECKO_PROFILE macros")]] ProfScope
{
  Label ScopeLabel {};
  u64 Time0 {0};
  const char* Name {nullptr};
  u32 NameHash {0};
  u32 ThreadId {0};
  ProfLevel Level {ProfLevel::Normal};
  u8 Category {0};
  bool Enabled {false};

  ProfScope(Label label, u32 hash, const char* name, ProfLevel level, u8 category = 0) noexcept
      : ScopeLabel(label), Name(name), NameHash(hash), ThreadId(ThisThreadId()), Level(level), Category(category)
  {
    if (!IsProfilerLevelEnabled(level))
      return;
    Enabled = true;
    Time0 = ProfilerNowNs();
    EmitProfileEvent({.TimestampNs = Time0,
                      .Name = Name,
                      .EventLabel = ScopeLabel,
                      .ThreadId = ThreadId,
                      .NameHash = NameHash,
                      .Kind = ProfEventKind::ZoneBegin,
                      .Level = Level,
                      .Source = ProfSource::CPU,
                      .Category = Category});
  }

  ~ProfScope() noexcept
  {
    if (Enabled)
      EmitProfileEvent({.TimestampNs = ProfilerNowNs(),
                        .Name = Name,
                        .EventLabel = ScopeLabel,
                        .ThreadId = ThreadId,
                        .NameHash = NameHash,
                        .Kind = ProfEventKind::ZoneEnd,
                        .Level = Level,
                        .Source = ProfSource::CPU,
                        .Category = Category});
  }

  ProfScope(const ProfScope&) = delete;
  ProfScope& operator=(const ProfScope&) = delete;
};

}  // namespace gecko

#ifndef GECKO_PROFILING
#define GECKO_PROFILING 1
#endif

#define GECKO_PROF_CONCAT_INNER(first, second) first##second
#define GECKO_PROF_CONCAT(first, second) GECKO_PROF_CONCAT_INNER(first, second)
#define GECKO_PROF_SCOPE(label, name, level, category)            \
  gecko::ProfScope GECKO_PROF_CONCAT(g_ProfScope_, __LINE__)      \
  {                                                               \
    (label), gecko::FNV1aLiteral(name), name, (level), (category) \
  }

#if GECKO_PROFILING && GECKO_PROF_MAX_LEVEL >= GECKO_PROF_LEVEL_DETAILED
#define GECKO_PROFILE(label)                                              \
  gecko::ProfScope GECKO_PROF_CONCAT(g_ProfScope_, __LINE__)              \
  {                                                                       \
    (label), gecko::FNV1a(__func__), __func__, gecko::ProfLevel::Detailed \
  }
#define GECKO_PROFILE_NAMED(label, name) GECKO_PROF_SCOPE(label, name, gecko::ProfLevel::Detailed, 0)
#define GECKO_PROFILE_CAT(label, name, category) \
  GECKO_PROF_SCOPE(label, name, gecko::ProfLevel::Detailed, static_cast<gecko::u8>(category))
#else
#define GECKO_PROFILE(label) (void)0
#define GECKO_PROFILE_NAMED(label, name) (void)0
#define GECKO_PROFILE_CAT(label, name, category) (void)0
#endif

#if GECKO_PROFILING && GECKO_PROF_MAX_LEVEL >= GECKO_PROF_LEVEL_NORMAL
#define GECKO_PROFILE_NORMAL(label)                                     \
  gecko::ProfScope GECKO_PROF_CONCAT(g_ProfScope_, __LINE__)            \
  {                                                                     \
    (label), gecko::FNV1a(__func__), __func__, gecko::ProfLevel::Normal \
  }
#define GECKO_PROFILE_NORMAL_NAMED(label, name) GECKO_PROF_SCOPE(label, name, gecko::ProfLevel::Normal, 0)
#define GECKO_PROFILE_NORMAL_CAT(label, name, category) \
  GECKO_PROF_SCOPE(label, name, gecko::ProfLevel::Normal, static_cast<gecko::u8>(category))
#else
#define GECKO_PROFILE_NORMAL(label) (void)0
#define GECKO_PROFILE_NORMAL_NAMED(label, name) (void)0
#define GECKO_PROFILE_NORMAL_CAT(label, name, category) (void)0
#endif

#if GECKO_PROFILING
#define GECKO_PROFILE_ALWAYS(label)                                     \
  gecko::ProfScope GECKO_PROF_CONCAT(g_ProfScope_, __LINE__)            \
  {                                                                     \
    (label), gecko::FNV1a(__func__), __func__, gecko::ProfLevel::Always \
  }
#define GECKO_PROFILE_ALWAYS_NAMED(label, name) GECKO_PROF_SCOPE(label, name, gecko::ProfLevel::Always, 0)
#define GECKO_PROFILE_ALWAYS_CAT(label, name, category) \
  GECKO_PROF_SCOPE(label, name, gecko::ProfLevel::Always, static_cast<gecko::u8>(category))
#define GECKO_COUNTER(label, name, value)                           \
  gecko::EmitProfileEvent({.TimestampNs = gecko::ProfilerNowNs(),   \
                           .Value = static_cast<gecko::u64>(value), \
                           .Name = name,                            \
                           .EventLabel = (label),                   \
                           .ThreadId = gecko::ThisThreadId(),       \
                           .NameHash = gecko::FNV1aLiteral(name),   \
                           .Kind = gecko::ProfEventKind::Counter})
#define GECKO_FRAME(label, name)                                  \
  gecko::EmitProfileEvent({.TimestampNs = gecko::ProfilerNowNs(), \
                           .Name = name,                          \
                           .EventLabel = (label),                 \
                           .ThreadId = gecko::ThisThreadId(),     \
                           .NameHash = gecko::FNV1aLiteral(name), \
                           .Kind = gecko::ProfEventKind::FrameMark})
#else
#define GECKO_PROFILE_ALWAYS(label) (void)0
#define GECKO_PROFILE_ALWAYS_NAMED(label, name) (void)0
#define GECKO_PROFILE_ALWAYS_CAT(label, name, category) (void)0
#define GECKO_COUNTER(label, name, value) (void)0
#define GECKO_FRAME(label, name) (void)0
#endif
