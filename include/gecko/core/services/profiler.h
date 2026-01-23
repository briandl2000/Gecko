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
};

GECKO_API IProfiler* GetProfiler() noexcept;
GECKO_API u32 ThisThreadId() noexcept;

struct ProfScope
{
  Label ScopeLabel {};                  // 16 bytes
  u64 Time0 {0};                        // 8 bytes
  const char* Name {nullptr};           // 8 bytes (allows custom name vs label)
  u32 NameHash {0};                     // 4 bytes
  u32 ThreadId {0};                     // 4 bytes
  ProfLevel Level {ProfLevel::Normal};  // 1 byte
  bool Enabled {false};                 // 1 byte
  // 6 bytes padding -> 48 bytes total

  ProfScope(Label label, u32 hash, const char* name, ProfLevel lvl) noexcept;
  ~ProfScope() noexcept;
  ProfScope(const ProfScope&) = delete;
  ProfScope& operator=(const ProfScope&) = delete;
};

}  // namespace gecko

#ifndef GECKO_PROFILING
#define GECKO_PROFILING 1
#endif

#if GECKO_PROFILING

#if GECKO_PROF_MAX_LEVEL >= GECKO_PROF_LEVEL_NORMAL
#define GECKO_PROF_SCOPE(label)                                             \
  ::gecko::ProfScope _g_prof                                                \
  {                                                                         \
    (label), ::gecko::FNV1a(__func__), __func__, ::gecko::ProfLevel::Normal \
  }
#define GECKO_PROF_SCOPE_NAMED(label, name)                                \
  ::gecko::ProfScope _g_prof                                               \
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
  ::gecko::ProfScope _g_prof                                                  \
  {                                                                           \
    (label), ::gecko::FNV1a(__func__), __func__, ::gecko::ProfLevel::Detailed \
  }
#define GECKO_PROF_SCOPE_NAMED_DETAILED(label, name)                         \
  ::gecko::ProfScope _g_prof                                                 \
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
  ::gecko::ProfScope _g_prof                                                \
  {                                                                         \
    (label), ::gecko::FNV1a(__func__), __func__, ::gecko::ProfLevel::Always \
  }
#define GECKO_PROF_SCOPE_NAMED_MARK(label, name)                           \
  ::gecko::ProfScope _g_prof                                               \
  {                                                                        \
    (label), ::gecko::FNV1aLiteral(name), name, ::gecko::ProfLevel::Always \
  }
#define GECKO_PROF_FUNC_MARK(label) GECKO_PROF_SCOPE_MARK(label)

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
#define GECKO_COUNTER(label, name, val) (void)0
#define GECKO_FRAME(label, name) (void)0

#endif  // GECKO_PROFILING

namespace gecko {

inline ProfScope::ProfScope(Label label, u32 hash, const char* name,
                            ProfLevel lvl) noexcept
    : ScopeLabel(label), Name(name), NameHash(hash), ThreadId(ThisThreadId()),
      Level(lvl)
{
  if (auto* prof = GetProfiler(); prof && prof->IsLevelEnabled(Level))
  {
    Enabled = true;
    Time0 = prof->NowNs();
    prof->Emit({.TimestampNs = Time0,
                .Name = Name,
                .EventLabel = ScopeLabel,
                .ThreadId = ThreadId,
                .NameHash = NameHash,
                .Kind = ProfEventKind::ZoneBegin,
                .Level = Level});
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
                  .Level = Level});
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
};

}  // namespace gecko
