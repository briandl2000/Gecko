#include "gecko/core/services.h"

#include "gecko/core/services/events.h"
#include "gecko/core/services/log.h"
#include "gecko/core/services/profiler.h"
#include "gecko/core/string.h"
#include "private/services.h"

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace gecko {

namespace {

NullJobSystem g_NullJobs;
NullProfiler g_NullProfiler;
NullLogger g_NullLogger;
NullEventBus g_NullEvents;

IJobSystem* g_Jobs = nullptr;
IProfiler* g_Profiler = nullptr;
ILogger* g_Logger = nullptr;
IEventBus* g_Events = nullptr;

template <typename T>
void StoreService(T*& destination, T* service) noexcept
{
#if defined(_MSC_VER)
  (void)_InterlockedExchangePointer(reinterpret_cast<void* volatile*>(&destination), service);
#else
  __atomic_store_n(&destination, service, __ATOMIC_RELEASE);
#endif
}

template <typename T>
T* LoadService(T* const& service) noexcept
{
#if defined(_MSC_VER)
  return static_cast<T*>(
      _InterlockedCompareExchangePointer(reinterpret_cast<void* volatile*>(const_cast<T**>(&service)), nullptr, nullptr));
#else
  return __atomic_load_n(&service, __ATOMIC_ACQUIRE);
#endif
}

struct ThreadNameEntry
{
  StaticString<63> Name;
  u32 ThreadId {0};
  bool Active {false};
};

ThreadNameEntry g_ThreadNames[64] {};
u32 g_ThreadNameLock = 0;
thread_local StaticString<63> g_CurrentThreadName;

void LockThreadNames() noexcept
{
#if defined(_MSC_VER)
  while (_InterlockedExchange(reinterpret_cast<volatile long*>(&g_ThreadNameLock), 1) != 0)
  {}
#else
  while (__atomic_exchange_n(&g_ThreadNameLock, 1U, __ATOMIC_ACQUIRE) != 0)
  {}
#endif
}

void UnlockThreadNames() noexcept
{
#if defined(_MSC_VER)
  (void)_InterlockedExchange(reinterpret_cast<volatile long*>(&g_ThreadNameLock), 0);
#else
  __atomic_store_n(&g_ThreadNameLock, 0U, __ATOMIC_RELEASE);
#endif
}

}  // namespace

namespace detail {

void SetRuntimeServices(IJobSystem* jobs, IProfiler* profiler, ILogger* logger, IEventBus* events) noexcept
{
  StoreService(g_Jobs, jobs);
  StoreService(g_Profiler, profiler);
  StoreService(g_Logger, logger);
  StoreService(g_Events, events);
}

}  // namespace detail

IJobSystem* GetJobSystem() noexcept
{
  IJobSystem* service = LoadService(g_Jobs);
  return service != nullptr ? service : &g_NullJobs;
}

IProfiler* GetProfiler() noexcept
{
  IProfiler* service = LoadService(g_Profiler);
  return service != nullptr ? service : &g_NullProfiler;
}

ILogger* GetLogger() noexcept
{
  ILogger* service = LoadService(g_Logger);
  return service != nullptr ? service : &g_NullLogger;
}

IEventBus* GetEventBus() noexcept
{
  IEventBus* service = LoadService(g_Events);
  return service != nullptr ? service : &g_NullEvents;
}

void SetThreadProfilerName(const char* name) noexcept
{
  g_CurrentThreadName.Clear();
  if (name != nullptr)
    g_CurrentThreadName.Append(StringView {name}.Substring(0, 63));
  RegisterThreadProfilerName(ThisThreadId(), name);
}

const char* GetThreadProfilerName() noexcept
{
  return g_CurrentThreadName.Count() != 0 ? g_CurrentThreadName.Data() : nullptr;
}

const char* LookupThreadProfilerName(u32 threadId) noexcept
{
  LockThreadNames();
  const char* result = nullptr;
  for (const ThreadNameEntry& entry : g_ThreadNames)
  {
    if (entry.Active && entry.ThreadId == threadId)
    {
      result = entry.Name.Data();
      break;
    }
  }
  UnlockThreadNames();
  return result;
}

void RegisterThreadProfilerName(u32 threadId, const char* name) noexcept
{
  LockThreadNames();
  ThreadNameEntry* destination = nullptr;
  for (ThreadNameEntry& entry : g_ThreadNames)
  {
    if (entry.Active && entry.ThreadId == threadId)
    {
      destination = &entry;
      break;
    }
    if (!entry.Active && destination == nullptr)
      destination = &entry;
  }
  if (destination != nullptr)
  {
    destination->Name.Clear();
    destination->ThreadId = threadId;
    destination->Active = name != nullptr;
    if (name != nullptr)
      destination->Name.Append(StringView {name}.Substring(0, 63));
  }
  UnlockThreadNames();
}

}  // namespace gecko
