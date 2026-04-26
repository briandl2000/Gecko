#pragma once
#include "gecko/core/api.h"
#include "gecko/core/labels.h"
#include "gecko/core/sink_registration.h"
#include "gecko/core/types.h"

namespace gecko {

enum class ProfLevel : u8
{
  Always = 0,
  Normal = 1,
  Detailed = 2,
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

enum class ProfEventKind : u8
{
  ZoneBegin,
  ZoneEnd,
  Counter,
  FrameMark
};

enum class ProfSource : u8
{
  CPU = 0,
  GPU = 1,
};

// Category 0 means "uncategorized". Modules call IProfiler::RegisterCategory()
// to obtain a stable u8 id (1..63) and pass it via GECKO_PROF_SCOPE_CAT.
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

static_assert(sizeof(ProfEvent) == 64, "ProfEvent must be 64 bytes");

struct IProfiler;

struct IProfilerSink : public RegisteredSink<IProfilerSink, IProfiler>
{
  virtual ~IProfilerSink() = default;
  virtual void Write(const ProfEvent& event) noexcept = 0;
  virtual void WriteBatch(const ProfEvent* events,
                          std::size_t count) noexcept = 0;
  virtual void Flush() noexcept = 0;
};

// Per-frame aggregator slot. Updated on ZoneEnd for Always-level scopes only.
// Reset on FrameMark. Read with GetStats(nameHash) for cheap HUD/telemetry.
struct ScopeStats
{
  u64 LastNs {0};
  u64 MinNs {~u64 {0}};
  u64 MaxNs {0};
  u32 Count {0};
};

struct ProfilerDiagnostics
{
  u64 DroppedEvents {0};
  u64 ReentrantDrops {0};
  u64 AggregatorOverflow {0};
};

constexpr u8 c_ProfMaxCategories = 64;
constexpr u8 c_ProfInvalidCategory = 0xFF;

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

  // Aggregator query. Returns a snapshot of the current frame's stats for
  // the given name-hash. Returns a default-constructed ScopeStats if the
  // scope has not been seen this frame.
  virtual ScopeStats GetStats(u32 nameHash) const noexcept = 0;

  // Categories. RegisterCategory returns a stable id (1..63) for the given
  // name; subsequent calls with the same name return the same id. Returns
  // c_ProfInvalidCategory on overflow.
  virtual u8 RegisterCategory(const char* name) noexcept = 0;
  virtual void SetCategoryEnabled(u8 id, bool on) noexcept = 0;
  virtual bool IsCategoryEnabled(u8 id) const noexcept = 0;

  virtual ProfilerDiagnostics GetDiagnostics() const noexcept = 0;
};

GECKO_API IProfiler* GetProfiler() noexcept;
GECKO_API u32 ThisThreadId() noexcept;

// Optional human-readable name for this thread. Stored in TLS; sinks emit
// a Chrome-trace 'thread_name' metadata record on first sight per thread.
GECKO_API void SetThreadProfilerName(const char* name) noexcept;
GECKO_API const char* GetThreadProfilerName() noexcept;

struct ProfScope
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

  ProfScope(Label label, u32 hash, const char* name, ProfLevel lvl,
            u8 cat = 0) noexcept;
  ~ProfScope() noexcept;
  ProfScope(const ProfScope&) = delete ("ProfScope is a stack-only RAII guard");
  ProfScope& operator=(const ProfScope&) =
      delete ("ProfScope is a stack-only RAII guard");
};

}  // namespace gecko

#ifndef GECKO_PROFILING
#define GECKO_PROFILING 1
#endif

#if GECKO_PROFILING

#define GECKO_PROF_CONCAT_(x, y) x##y
#define GECKO_PROF_CONCAT(x, y) GECKO_PROF_CONCAT_(x, y)

#if GECKO_PROF_MAX_LEVEL >= GECKO_PROF_LEVEL_NORMAL
#define GECKO_PROF_SCOPE(label)                                             \
  ::gecko::ProfScope GECKO_PROF_CONCAT(_g_prof_, __LINE__)                  \
  {                                                                         \
    (label), ::gecko::FNV1a(__func__), __func__, ::gecko::ProfLevel::Normal \
  }
#define GECKO_PROF_SCOPE_NAMED(label, name)                                \
  ::gecko::ProfScope GECKO_PROF_CONCAT(_g_prof_, __LINE__)                 \
  {                                                                        \
    (label), ::gecko::FNV1aLiteral(name), name, ::gecko::ProfLevel::Normal \
  }
#define GECKO_PROF_FUNC(label) GECKO_PROF_SCOPE(label)
#else
#define GECKO_PROF_SCOPE(label) (void)0
#define GECKO_PROF_SCOPE_NAMED(label, name) (void)0
#define GECKO_PROF_FUNC(label) (void)0
#endif

#if GECKO_PROF_MAX_LEVEL >= GECKO_PROF_LEVEL_DETAILED
#define GECKO_PROF_SCOPE_DETAILED(label)                                      \
  ::gecko::ProfScope GECKO_PROF_CONCAT(_g_prof_, __LINE__)                    \
  {                                                                           \
    (label), ::gecko::FNV1a(__func__), __func__, ::gecko::ProfLevel::Detailed \
  }
#define GECKO_PROF_SCOPE_NAMED_DETAILED(label, name)                         \
  ::gecko::ProfScope GECKO_PROF_CONCAT(_g_prof_, __LINE__)                   \
  {                                                                          \
    (label), ::gecko::FNV1aLiteral(name), name, ::gecko::ProfLevel::Detailed \
  }
#define GECKO_PROF_FUNC_DETAILED(label) GECKO_PROF_SCOPE_DETAILED(label)
#else
#define GECKO_PROF_SCOPE_DETAILED(label) (void)0
#define GECKO_PROF_SCOPE_NAMED_DETAILED(label, name) (void)0
#define GECKO_PROF_FUNC_DETAILED(label) (void)0
#endif

#define GECKO_PROF_SCOPE_MARK(label)                                        \
  ::gecko::ProfScope GECKO_PROF_CONCAT(_g_prof_, __LINE__)                  \
  {                                                                         \
    (label), ::gecko::FNV1a(__func__), __func__, ::gecko::ProfLevel::Always \
  }
#define GECKO_PROF_SCOPE_NAMED_MARK(label, name)                           \
  ::gecko::ProfScope GECKO_PROF_CONCAT(_g_prof_, __LINE__)                 \
  {                                                                        \
    (label), ::gecko::FNV1aLiteral(name), name, ::gecko::ProfLevel::Always \
  }
#define GECKO_PROF_FUNC_MARK(label) GECKO_PROF_SCOPE_MARK(label)

// Category-tagged variants. `cat` is a u8 returned from
// IProfiler::RegisterCategory(); category 0 is the always-enabled default.
#if GECKO_PROF_MAX_LEVEL >= GECKO_PROF_LEVEL_NORMAL
#define GECKO_PROF_SCOPE_NAMED_CAT(label, name, cat)                        \
  ::gecko::ProfScope GECKO_PROF_CONCAT(_g_prof_, __LINE__)                  \
  {                                                                         \
    (label), ::gecko::FNV1aLiteral(name), name, ::gecko::ProfLevel::Normal, \
        (::gecko::u8)(cat)                                                  \
  }
#else
#define GECKO_PROF_SCOPE_NAMED_CAT(label, name, cat) (void)0
#endif

#if GECKO_PROF_MAX_LEVEL >= GECKO_PROF_LEVEL_DETAILED
#define GECKO_PROF_SCOPE_NAMED_CAT_DETAILED(label, name, cat)                 \
  ::gecko::ProfScope GECKO_PROF_CONCAT(_g_prof_, __LINE__)                    \
  {                                                                           \
    (label), ::gecko::FNV1aLiteral(name), name, ::gecko::ProfLevel::Detailed, \
        (::gecko::u8)(cat)                                                    \
  }
#else
#define GECKO_PROF_SCOPE_NAMED_CAT_DETAILED(label, name, cat) (void)0
#endif

#define GECKO_PROF_SCOPE_NAMED_CAT_MARK(label, name, cat)                   \
  ::gecko::ProfScope GECKO_PROF_CONCAT(_g_prof_, __LINE__)                  \
  {                                                                         \
    (label), ::gecko::FNV1aLiteral(name), name, ::gecko::ProfLevel::Always, \
        (::gecko::u8)(cat)                                                  \
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

#define GECKO_PROF_SCOPE(label) (void)0
#define GECKO_PROF_SCOPE_NAMED(label, name) (void)0
#define GECKO_PROF_FUNC(label) (void)0
#define GECKO_PROF_SCOPE_DETAILED(label) (void)0
#define GECKO_PROF_SCOPE_NAMED_DETAILED(label, name) (void)0
#define GECKO_PROF_FUNC_DETAILED(label) (void)0
#define GECKO_PROF_SCOPE_MARK(label) (void)0
#define GECKO_PROF_SCOPE_NAMED_MARK(label, name) (void)0
#define GECKO_PROF_FUNC_MARK(label) (void)0
#define GECKO_PROF_SCOPE_NAMED_CAT(label, name, cat) (void)0
#define GECKO_PROF_SCOPE_NAMED_CAT_DETAILED(label, name, cat) (void)0
#define GECKO_PROF_SCOPE_NAMED_CAT_MARK(label, name, cat) (void)0
#define GECKO_COUNTER(label, name, val) (void)0
#define GECKO_FRAME(label, name) (void)0

#endif  // GECKO_PROFILING

namespace gecko {

inline ProfScope::ProfScope(Label label, u32 hash, const char* name,
                            ProfLevel lvl, u8 cat) noexcept
    : ScopeLabel(label), Name(name), NameHash(hash), ThreadId(ThisThreadId()),
      Level(lvl), Category(cat)
{
  if (auto* prof = GetProfiler();
      prof && prof->IsLevelEnabled(Level) && prof->IsCategoryEnabled(Category))
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
  ScopeStats GetStats(u32) const noexcept override
  {
    return {};
  }
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
  ProfilerDiagnostics GetDiagnostics() const noexcept override
  {
    return {};
  }
};

}  // namespace gecko
