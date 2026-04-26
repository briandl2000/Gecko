#include "gecko/core/services.h"

#include "gecko/core/assert.h"
#include "gecko/core/services/events.h"
#include "gecko/core/services/log.h"
#include "gecko/core/services/profiler.h"
#include "private/labels.h"

#include <atomic>
#include <mutex>
#include <unordered_map>

namespace gecko {

static NullJobSystem s_NullJobSystem;
static NullProfiler s_NullProfiler;
static NullLogger s_NullLogger;
static NullEventBus s_NullEventBus;
static NullModuleRegistry s_NullModuleRegistry;

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

// Active module registry, owned by gecko::Engine. Set by Engine::Create
// (via SetActiveModuleRegistry) and cleared on Engine destruction. All
// service accessors route through this — the engine is the single source
// of truth for which implementation is live.
static std::atomic<IModuleRegistry*> g_Modules {nullptr};

namespace detail {

void SetActiveModuleRegistry(IModuleRegistry* registry) noexcept
{
  g_Modules.store(registry, std::memory_order_release);
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

IModuleRegistry* GetModules() noexcept
{
  auto* m = g_Modules.load(std::memory_order_acquire);
  return m ? m : &s_NullModuleRegistry;
}

IJobSystem* GetJobSystem() noexcept
{
  if (auto* m = g_Modules.load(std::memory_order_acquire))
  {
    if (auto* impl = m->Service<IJobSystem>())
      return impl;
  }
  return &s_NullJobSystem;
}

IProfiler* GetProfiler() noexcept
{
  if (auto* m = g_Modules.load(std::memory_order_acquire))
  {
    if (auto* impl = m->Service<IProfiler>())
      return impl;
  }
  return &s_NullProfiler;
}

namespace {
thread_local const char* tls_ThreadProfilerName = nullptr;

// Process-global registry: TID-hash -> profiler name. Written by
// SetThreadProfilerName; read by trace sinks emitting chrome-trace
// `thread_name` metadata. The map only ever holds named threads (small).
std::mutex g_ThreadNameMu;
std::unordered_map<u32, const char*> g_ThreadNameMap;
}  // namespace

void SetThreadProfilerName(const char* name) noexcept
{
  tls_ThreadProfilerName = name;
  if (!name)
    return;
  u32 tid = ThisThreadId();
  std::lock_guard<std::mutex> lk(g_ThreadNameMu);
  g_ThreadNameMap[tid] = name;
}

const char* GetThreadProfilerName() noexcept
{
  return tls_ThreadProfilerName;
}

const char* LookupThreadProfilerName(u32 threadId) noexcept
{
  std::lock_guard<std::mutex> lk(g_ThreadNameMu);
  auto it = g_ThreadNameMap.find(threadId);
  return (it != g_ThreadNameMap.end()) ? it->second : nullptr;
}

ILogger* GetLogger() noexcept
{
  if (auto* m = g_Modules.load(std::memory_order_acquire))
  {
    if (auto* impl = m->Service<ILogger>())
      return impl;
  }
  return &s_NullLogger;
}

IEventBus* GetEventBus() noexcept
{
  if (auto* m = g_Modules.load(std::memory_order_acquire))
  {
    if (auto* impl = m->Service<IEventBus>())
      return impl;
  }
  return &s_NullEventBus;
}

}  // namespace gecko
