#pragma once

#include "gecko/core/api.h"
#include "gecko/core/labels.h"
#include "gecko/core/sink_registration.h"
#include "gecko/core/types.h"

namespace gecko {

enum class ProfEventKind : u8
{
  ZoneBegin,
  ZoneEnd,
  Counter,
  FrameMark
};

// aligned to speed up profile events
struct alignas(64) ProfEvent
{
  u64 TimestampNs {0};
  u64 Value {0};
  const char* Name {nullptr};
  Label EventLabel {};

  u32 ThreadId {0};
  u32 NameHash {0};

  ProfEventKind Kind {ProfEventKind::ZoneBegin};
};

static_assert(sizeof(ProfEvent) == 64,
              "ProfEvent must be 64 bytes (cache line size)");
static_assert(alignof(ProfEvent) == 64, "ProfEvent must be cache-line aligned");

// Forward declare for RegisteredSink
struct IProfiler;

struct IProfilerSink : public RegisteredSink<IProfilerSink, IProfiler>
{
  GECKO_API virtual ~IProfilerSink() = default;

  GECKO_API virtual void Write(const ProfEvent& event) noexcept = 0;

  GECKO_API virtual void WriteBatch(const ProfEvent* events,
                                    ::std::size_t count) noexcept = 0;

  GECKO_API virtual void Flush() noexcept = 0;
};

struct IProfiler
{
  GECKO_API virtual ~IProfiler() = default;

  GECKO_API virtual void Emit(const ProfEvent& ev) noexcept = 0;
  GECKO_API virtual u64 NowNs() const noexcept = 0;

  // Internal: called by RegisteredSink
  GECKO_API virtual void AddSink(IProfilerSink* sink) noexcept = 0;
  GECKO_API virtual void RemoveSink(IProfilerSink* sink) noexcept = 0;

  GECKO_API virtual bool Init() noexcept = 0;
  GECKO_API virtual void Shutdown() noexcept = 0;
};

GECKO_API IProfiler* GetProfiler() noexcept;

GECKO_API u32 ThisThreadId() noexcept;

struct ProfScope
{
  Label ScopeLabel {};
  u64 Time0 {0};
  const char* Name {nullptr};
  u32 NameHash {0};
  u32 TimeId {0};

  ProfScope(Label labelIn, u32 nameHash, const char* namePtr) noexcept;
  ~ProfScope() noexcept;
};
}  // namespace gecko

#ifndef GECKO_PROFILING
#define GECKO_PROFILING 1
#endif

#if GECKO_PROFILING
// Note: GECKO_PROF_FUNC uses FNV1a (runtime) because __func__ is not a literal
#define GECKO_PROF_FUNC(label)                  \
  ::gecko::ProfScope _g_scope                   \
  {                                             \
    (label), ::gecko::FNV1a(__func__), __func__ \
  }

// Use FNV1aLiteral for compile-time hash of string literal names
#define GECKO_PROF_SCOPE(label, name)         \
  ::gecko::ProfScope _g_scope                 \
  {                                           \
    (label), ::gecko::FNV1aLiteral(name), name \
  }

#define GECKO_PROF_COUNTER(label, name, val)                              \
  do                                                                      \
  {                                                                       \
    if (auto* p = ::gecko::GetProfiler())                                 \
    {                                                                     \
      ::gecko::ProfEvent event {.TimestampNs = p->NowNs(),                \
                                .Value = (::gecko::u64)(val),             \
                                .Name = name,                             \
                                .EventLabel = (label),                    \
                                .ThreadId = ::gecko::ThisThreadId(),      \
                                .NameHash = ::gecko::FNV1aLiteral(name),  \
                                .Kind = ::gecko::ProfEventKind::Counter}; \
      p->Emit(event);                                                     \
    }                                                                     \
  } while (0)

#define GECKO_PROF_FRAME(label, name)                                       \
  do                                                                        \
  {                                                                         \
    if (auto* p = ::gecko::GetProfiler())                                   \
    {                                                                       \
      ::gecko::ProfEvent event {.TimestampNs = p->NowNs(),                  \
                                .Value = 0,                                 \
                                .Name = name,                               \
                                .EventLabel = (label),                      \
                                .ThreadId = ::gecko::ThisThreadId(),        \
                                .NameHash = ::gecko::FNV1aLiteral(name),    \
                                .Kind = ::gecko::ProfEventKind::FrameMark}; \
      p->Emit(event);                                                       \
    }                                                                       \
  } while (0)
#else
#define GECKO_PROF_CATEGORY(x)
#define GECKO_PROF_FUNC(x)
#define GECKO_PROF_ZONE(x, y)
#define GECKO_PROF_COUNTER(x, y, z)
#define GECKO_PROF_FRAME(x)
#endif  // GECKO_PROFILING

namespace gecko {

inline ProfScope::ProfScope(Label labelIn, u32 nameHash,
                            const char* namePtr) noexcept
    : ScopeLabel(labelIn), Time0(0), Name(namePtr), NameHash(nameHash),
      TimeId(ThisThreadId())
{
  if (auto* profiler = GetProfiler())
  {
    Time0 = profiler->NowNs();
    ProfEvent event {.TimestampNs = Time0,
                     .Value = 0,
                     .Name = Name,
                     .EventLabel = ScopeLabel,
                     .ThreadId = TimeId,
                     .NameHash = NameHash,
                     .Kind = ProfEventKind::ZoneBegin};
    profiler->Emit(event);
  }
}

inline ProfScope::~ProfScope() noexcept
{
  if (auto* profiler = GetProfiler())
  {
    ProfEvent event {.TimestampNs = profiler->NowNs(),
                     .Value = 0,
                     .Name = Name,
                     .EventLabel = ScopeLabel,
                     .ThreadId = TimeId,
                     .NameHash = NameHash,
                     .Kind = ProfEventKind::ZoneEnd};
    profiler->Emit(event);
  }
}
}  // namespace gecko

namespace gecko {
struct NullProfiler final : IProfiler
{
  GECKO_API virtual void Emit(const ProfEvent& event) noexcept override;
  GECKO_API virtual u64 NowNs() const noexcept override;
  GECKO_API virtual void AddSink(IProfilerSink* sink) noexcept override;
  GECKO_API virtual void RemoveSink(IProfilerSink* sink) noexcept override;

  GECKO_API virtual bool Init() noexcept override;
  GECKO_API virtual void Shutdown() noexcept override;
};

}  // namespace gecko
