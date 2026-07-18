#include "gecko/core/services.h"

#include "gecko/core/assert.h"
#include "gecko/core/services/events.h"
#include "gecko/core/services/log.h"
#include "gecko/core/services/profiler.h"
#include "private/labels.h"
#include "private/services.h"

#include <atomic>
#include <mutex>
#include <string>
#include <unordered_map>

namespace gecko {

static NullJobSystem s_NullJobSystem;
static NullProfiler s_NullProfiler;
static NullLogger s_NullLogger;
static NullEventBus s_NullEventBus;

// Function-local static avoids the static initialization order fiasco.
// The default SystemAllocator is alive from first use until process exit
// and is the fallback whenever no user allocator is installed.
static SystemAllocator& DefaultAllocator() noexcept
{
  static SystemAllocator instance;
  return instance;
}

// User-installed allocator (via SetAllocator). Null means "use the
// default". The Allocator() accessor never observes a torn state because
// SetAllocator/ResetAllocator publish via memory_order_release and readers
// load via memory_order_acquire.
static std::atomic<IAllocator*> g_UserAllocator {nullptr};

static std::atomic<IJobSystem*> g_Jobs {nullptr};
static std::atomic<IProfiler*> g_Profiler {nullptr};
static std::atomic<ILogger*> g_Logger {nullptr};
static std::atomic<IEventBus*> g_Events {nullptr};

namespace detail {

void SetRuntimeServices(IJobSystem* jobs, IProfiler* profiler, ILogger* logger, IEventBus* events) noexcept
{
  g_Jobs.store(jobs, std::memory_order_release);
  g_Profiler.store(profiler, std::memory_order_release);
  g_Logger.store(logger, std::memory_order_release);
  g_Events.store(events, std::memory_order_release);
}

}  // namespace detail

IAllocator& Allocator() noexcept
{
  if (auto* alloc = g_UserAllocator.load(std::memory_order_acquire))
    return *alloc;
  return DefaultAllocator();
}

bool SetAllocator(IAllocator* allocator) noexcept
{
  if (allocator == nullptr)
  {
    ResetAllocator();
    return true;
  }

  // Replace path: shut down whatever was previously installed first.
  if (auto* prev = g_UserAllocator.exchange(nullptr, std::memory_order_acq_rel))
  {
    prev->Shutdown();
  }

  if (!allocator->Init())
    return false;

  g_UserAllocator.store(allocator, std::memory_order_release);
  return true;
}

void ResetAllocator() noexcept
{
  if (auto* prev = g_UserAllocator.exchange(nullptr, std::memory_order_acq_rel))
  {
    prev->Shutdown();
  }
}

IJobSystem* GetJobSystem() noexcept
{
  if (auto* impl = g_Jobs.load(std::memory_order_acquire))
    return impl;
  return &s_NullJobSystem;
}

IProfiler* GetProfiler() noexcept
{
  if (auto* impl = g_Profiler.load(std::memory_order_acquire))
    return impl;
  return &s_NullProfiler;
}

namespace {
thread_local ::std::string tls_ThreadProfilerName;

// Process-global registry: TID -> profiler name. Written by
// SetThreadProfilerName; read by trace sinks emitting chrome-trace
// `thread_name` metadata. The map only ever holds named threads (small).
// Names are owned strings so callers may pass non-static buffers.
std::mutex g_ThreadNameMu;
std::unordered_map<u32, ::std::string> g_ThreadNameMap;
}  // namespace

void SetThreadProfilerName(const char* name) noexcept
{
  if (!name)
  {
    tls_ThreadProfilerName.clear();
    return;
  }
  tls_ThreadProfilerName = name;
  u32 tid = ThisThreadId();
  std::lock_guard<std::mutex> lk(g_ThreadNameMu);
  g_ThreadNameMap[tid] = name;
}

const char* GetThreadProfilerName() noexcept
{
  return tls_ThreadProfilerName.empty() ? nullptr : tls_ThreadProfilerName.c_str();
}

const char* LookupThreadProfilerName(u32 threadId) noexcept
{
  std::lock_guard<std::mutex> lk(g_ThreadNameMu);
  auto it = g_ThreadNameMap.find(threadId);
  return (it != g_ThreadNameMap.end()) ? it->second.c_str() : nullptr;
}

void RegisterThreadProfilerName(u32 threadId, const char* name) noexcept
{
  std::lock_guard<std::mutex> lk(g_ThreadNameMu);
  if (name == nullptr)
    g_ThreadNameMap.erase(threadId);
  else
    g_ThreadNameMap[threadId] = name;
}

ILogger* GetLogger() noexcept
{
  if (auto* impl = g_Logger.load(std::memory_order_acquire))
    return impl;
  return &s_NullLogger;
}

IEventBus* GetEventBus() noexcept
{
  if (auto* impl = g_Events.load(std::memory_order_acquire))
    return impl;
  return &s_NullEventBus;
}

}  // namespace gecko
