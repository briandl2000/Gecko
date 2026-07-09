#include "gecko/core/services.h"

#include "gecko/core/assert.h"
#include "gecko/core/services/events.h"
#include "gecko/core/services/log.h"
#include "gecko/core/services/profiler.h"
#include "private/labels.h"

#include <atomic>
#include <mutex>
#include <string>
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
// service accessors route through this -- the engine is the single source
// of truth for which implementation is live.
static std::atomic<IModuleRegistry*> g_Modules {nullptr};

constexpr u32 MaxAllocatorStackDepth = 32;

struct ThreadAllocatorContext
{
  IAllocator* Stack[MaxAllocatorStackDepth] {};
  u32 Depth {0};
};

thread_local ThreadAllocatorContext g_AllocatorContext;

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

IAllocator& CurrentAllocator() noexcept
{
  if (g_AllocatorContext.Depth > 0)
  {
    if (auto* alloc = g_AllocatorContext.Stack[g_AllocatorContext.Depth - 1])
      return *alloc;
  }
  return Allocator();
}

namespace detail {

bool PushCurrentAllocator(IAllocator* allocator) noexcept
{
  if (allocator == nullptr || g_AllocatorContext.Depth >= MaxAllocatorStackDepth)
    return false;
  g_AllocatorContext.Stack[g_AllocatorContext.Depth] = allocator;
  ++g_AllocatorContext.Depth;
  return true;
}

void PopCurrentAllocator() noexcept
{
  GECKO_ASSERT(g_AllocatorContext.Depth > 0 && "AllocatorPushScope pop without matching push");
  if (g_AllocatorContext.Depth == 0)
    return;
  --g_AllocatorContext.Depth;
  g_AllocatorContext.Stack[g_AllocatorContext.Depth] = nullptr;
}

}  // namespace detail

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
