#pragma once

#include "gecko/core/api.h"
#include "gecko/core/labels.h"
#include "gecko/core/types.h"
#include "gecko/core/utility/hash.h"

namespace gecko {

enum class ProfLevel : u8
{
  Always = 0,
  Normal = 1,
  Detailed = 2,
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

struct IProfiler
{
  using ForEachScopeFn = void (*)(const char* name, u32 nameHash, ProfSource source,
                                  const ScopeStats& stats, void* user);

  virtual ~IProfiler() = default;
  virtual void Emit(const ProfEvent& event) noexcept = 0;
  virtual u64 NowNs() const noexcept = 0;
  virtual void SetMinLevel(ProfLevel level) noexcept = 0;
  virtual ProfLevel GetMinLevel() const noexcept = 0;
  virtual bool IsLevelEnabled(ProfLevel level) const noexcept = 0;
  virtual ScopeStats GetStats(u32 nameHash, ProfSource source = ProfSource::CPU) const noexcept = 0;
  ScopeStats GetStats(const char* name, ProfSource source = ProfSource::CPU) const noexcept
  {
    return GetStats(name != nullptr ? FNV1a(name) : 0, source);
  }
  virtual void ResetStats() noexcept = 0;
  virtual void ForEachScope(ForEachScopeFn callback, void* user) const noexcept = 0;
  virtual void DumpStats(Label label) const noexcept = 0;
  virtual ProfilerDiagnostics GetDiagnostics() const noexcept = 0;
  virtual bool Init() noexcept = 0;
  virtual void Shutdown() noexcept = 0;
};

GECKO_API IProfiler* GetProfiler() noexcept;
GECKO_API u32 ThisThreadId() noexcept;
GECKO_API void SetThreadProfilerName(const char* name) noexcept;
GECKO_API const char* GetThreadProfilerName() noexcept;
GECKO_API const char* LookupThreadProfilerName(u32 threadId) noexcept;
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

  ProfScope(Label label, u32 hash, const char* name, ProfLevel level, u8 category = 0) noexcept;
  ~ProfScope() noexcept;
  ProfScope(const ProfScope&) = delete;
  ProfScope& operator=(const ProfScope&) = delete;
};

struct NullProfiler final : IProfiler
{
  void Emit(const ProfEvent&) noexcept override
  {}
  u64 NowNs() const noexcept override
  {
    return 0;
  }
  void SetMinLevel(ProfLevel level) noexcept override
  {
    m_Level = level;
  }
  ProfLevel GetMinLevel() const noexcept override
  {
    return m_Level;
  }
  bool IsLevelEnabled(ProfLevel level) const noexcept override
  {
    return level <= m_Level;
  }
  ScopeStats GetStats(u32, ProfSource) const noexcept override
  {
    return {};
  }
  void ResetStats() noexcept override
  {}
  void ForEachScope(ForEachScopeFn, void*) const noexcept override
  {}
  void DumpStats(Label) const noexcept override
  {}
  ProfilerDiagnostics GetDiagnostics() const noexcept override
  {
    return {};
  }
  bool Init() noexcept override
  {
    return true;
  }
  void Shutdown() noexcept override
  {}

  ProfLevel m_Level {ProfLevel::Normal};
};

inline ProfScope::ProfScope(Label label, u32 hash, const char* name, ProfLevel level, u8 category) noexcept
    : ScopeLabel(label), Name(name), NameHash(hash), ThreadId(ThisThreadId()), Level(level), Category(category)
{
  IProfiler* profiler = GetProfiler();
  if (profiler != nullptr && profiler->IsLevelEnabled(level))
  {
    Enabled = true;
    Time0 = profiler->NowNs();
    profiler->Emit({.TimestampNs = Time0,
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
  if (!Enabled)
    return;
  IProfiler* profiler = GetProfiler();
  profiler->Emit({.TimestampNs = profiler->NowNs(),
                  .Name = Name,
                  .EventLabel = ScopeLabel,
                  .ThreadId = ThreadId,
                  .NameHash = NameHash,
                  .Kind = ProfEventKind::ZoneEnd,
                  .Level = Level,
                  .Source = ProfSource::CPU,
                  .Category = Category});
}

}  // namespace gecko

#ifndef GECKO_PROFILING
#define GECKO_PROFILING 1
#endif

#define GECKO_PROF_CONCAT_INNER(first, second) first##second
#define GECKO_PROF_CONCAT(first, second) GECKO_PROF_CONCAT_INNER(first, second)

#if GECKO_PROFILING && GECKO_PROF_MAX_LEVEL >= GECKO_PROF_LEVEL_DETAILED
#define GECKO_PROFILE(label) \
  gecko::ProfScope GECKO_PROF_CONCAT(g_ProfScope_, __LINE__) {(label), gecko::FNV1a(__func__), __func__, gecko::ProfLevel::Detailed}
#define GECKO_PROFILE_NAMED(label, name) \
  gecko::ProfScope GECKO_PROF_CONCAT(g_ProfScope_, __LINE__) {(label), gecko::FNV1aLiteral(name), name, gecko::ProfLevel::Detailed}
#define GECKO_PROFILE_CAT(label, name, category) \
  gecko::ProfScope GECKO_PROF_CONCAT(g_ProfScope_, __LINE__) {(label), gecko::FNV1aLiteral(name), name, gecko::ProfLevel::Detailed, static_cast<gecko::u8>(category)}
#else
#define GECKO_PROFILE(label) (void)0
#define GECKO_PROFILE_NAMED(label, name) (void)0
#define GECKO_PROFILE_CAT(label, name, category) (void)0
#endif

#if GECKO_PROFILING && GECKO_PROF_MAX_LEVEL >= GECKO_PROF_LEVEL_NORMAL
#define GECKO_PROFILE_NORMAL(label) \
  gecko::ProfScope GECKO_PROF_CONCAT(g_ProfScope_, __LINE__) {(label), gecko::FNV1a(__func__), __func__, gecko::ProfLevel::Normal}
#define GECKO_PROFILE_NORMAL_NAMED(label, name) \
  gecko::ProfScope GECKO_PROF_CONCAT(g_ProfScope_, __LINE__) {(label), gecko::FNV1aLiteral(name), name, gecko::ProfLevel::Normal}
#define GECKO_PROFILE_NORMAL_CAT(label, name, category) \
  gecko::ProfScope GECKO_PROF_CONCAT(g_ProfScope_, __LINE__) {(label), gecko::FNV1aLiteral(name), name, gecko::ProfLevel::Normal, static_cast<gecko::u8>(category)}
#else
#define GECKO_PROFILE_NORMAL(label) (void)0
#define GECKO_PROFILE_NORMAL_NAMED(label, name) (void)0
#define GECKO_PROFILE_NORMAL_CAT(label, name, category) (void)0
#endif

#if GECKO_PROFILING
#define GECKO_PROFILE_ALWAYS(label) \
  gecko::ProfScope GECKO_PROF_CONCAT(g_ProfScope_, __LINE__) {(label), gecko::FNV1a(__func__), __func__, gecko::ProfLevel::Always}
#define GECKO_PROFILE_ALWAYS_NAMED(label, name) \
  gecko::ProfScope GECKO_PROF_CONCAT(g_ProfScope_, __LINE__) {(label), gecko::FNV1aLiteral(name), name, gecko::ProfLevel::Always}
#define GECKO_PROFILE_ALWAYS_CAT(label, name, category) \
  gecko::ProfScope GECKO_PROF_CONCAT(g_ProfScope_, __LINE__) {(label), gecko::FNV1aLiteral(name), name, gecko::ProfLevel::Always, static_cast<gecko::u8>(category)}
#define GECKO_COUNTER(label, name, value) \
  do { gecko::IProfiler* profiler = gecko::GetProfiler(); profiler->Emit({.TimestampNs = profiler->NowNs(), .Value = static_cast<gecko::u64>(value), .Name = name, .EventLabel = (label), .ThreadId = gecko::ThisThreadId(), .NameHash = gecko::FNV1aLiteral(name), .Kind = gecko::ProfEventKind::Counter}); } while (0)
#define GECKO_FRAME(label, name) \
  do { gecko::IProfiler* profiler = gecko::GetProfiler(); profiler->Emit({.TimestampNs = profiler->NowNs(), .Name = name, .EventLabel = (label), .ThreadId = gecko::ThisThreadId(), .NameHash = gecko::FNV1aLiteral(name), .Kind = gecko::ProfEventKind::FrameMark}); } while (0)
#else
#define GECKO_PROFILE_ALWAYS(label) (void)0
#define GECKO_PROFILE_ALWAYS_NAMED(label, name) (void)0
#define GECKO_PROFILE_ALWAYS_CAT(label, name, category) (void)0
#define GECKO_COUNTER(label, name, value) (void)0
#define GECKO_FRAME(label, name) (void)0
#endif
