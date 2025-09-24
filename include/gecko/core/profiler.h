#pragma once

#include <atomic>
#include <cstdint>

#include "api.h"
#include "category.h"
#include "types.h"


namespace gecko {

  enum class ProfEventKind : u8 { ZoneBegin, ZoneEnd, Counter, FrameMark };

  struct ProfEvent 
  {
    ProfEventKind Kind { ProfEventKind::ZoneBegin };
    u64 TimestampNs { 0 };
    u32 ThreadId { 0 };
    Category Cat { 0 };
    u32 NameHash { 0 };
    const char* Name { nullptr };
    u64 Value { 0 };
  };

  struct IProfiler 
  {
    GECKO_API virtual ~IProfiler() = default;

    GECKO_API virtual void Emit(const ProfEvent& ev) noexcept = 0;
    GECKO_API virtual u64 NowNs() const noexcept = 0;

    GECKO_API virtual bool Init() noexcept = 0;
    GECKO_API virtual void Shutdown() noexcept = 0;
  };

  GECKO_API IProfiler* GetProfiler() noexcept; 
  
  GECKO_API u32 ThisThreadId() noexcept;
  
  struct ProfScope
  {
    Category Cat { 0 };
    u32 NameHash { 0 };
    const char* Name { nullptr };
    u32 TimeId { 0 };
    u64 Time0 { 0 };

    ProfScope(Category category, u32 nameHash, const char* namePtr) noexcept;
    ~ProfScope() noexcept;
  };
}

#ifndef GECKO_PROFILING
  #define GECKO_PROFILING 1
#endif

#if GECKO_PROFILING
  #define GECKO_PROF_FUNC(cat)   \
    ::gecko::ProfScope _g_scope{ \
      (cat),                     \
      ::gecko::FNV1a(__func__),  \
      __func__                   \
    }

  #define GECKO_PROF_SCOPE(cat, name)  \
    ::gecko::ProfScope _g_scope{       \
      (cat),                           \
      ::gecko::FNV1a(name),            \
      name                             \
    }

  #define GECKO_PROF_COUNTER(cat, name, val)      \
    do { if( auto* p = ::gecko::GetProfiler()) {  \
      ::gecko::ProfEvent event{                   \
        ::gecko::ProfEventKind::Counter,          \
        p->NowNs(), ::gecko::ThisThreadId(),      \
        (cat),                                    \
        ::gecko::FNV1a(name),                     \
        name,                                     \
        (::gecko::u64)(val)                       \
      };                                          \
      p->Emit(event);                             \
    } } while(0)

  #define GECKO_PROF_FRAME(cat, name)            \
    do { if (auto *p = ::gecko::GetProfiler()) { \
      ::gecko::ProfEvent event{                  \
        ::gecko::ProfEventKind::FrameMark,       \
        p->NowNs(), ::gecko::ThisThreadId(),     \
        (cat),                                   \
        ::gecko::FNV1a(name),                    \
        name,                                    \
        0                                        \
      };                                         \
      p->Emit(event);                            \
    } } while(0)
#else
  #define GECKO_PROF_CATEGORY(x)
  #define GECKO_PROF_FUNC(x)
  #define GECKO_PROF_ZONE(x, y)
  #define GECKO_PROF_COUNTER(x, y, z)
  #define GECKO_PROF_FRAME(x)
#endif // GECKO_PROFILING

namespace gecko {

  inline ProfScope::ProfScope(Category category, u32 nameHash, const char* namePtr) noexcept
    : Cat(category)
    , NameHash(nameHash)
    , Name(namePtr)
    , TimeId(ThisThreadId())
    , Time0(0)
  {
    if (auto* profiler = GetProfiler())
    {
      Time0 = profiler->NowNs();
      ProfEvent event { ProfEventKind::ZoneBegin, Time0, TimeId, Cat, NameHash, Name, 0};
      profiler->Emit(event);
    }
  }

  inline ProfScope::~ProfScope() noexcept 
  {
    if(auto* profiler = GetProfiler())
    {
      ProfEvent event {ProfEventKind::ZoneEnd, profiler->NowNs(), TimeId, Cat, NameHash, Name, 0};
      profiler->Emit(event);
    }
  }
}
