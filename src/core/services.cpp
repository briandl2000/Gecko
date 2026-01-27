#include "gecko/core/services.h"

#include "gecko/core/assert.h"
#include "gecko/core/services/log.h"
#include "gecko/core/services/profiler.h"
#include "private/labels.h"

#include <atomic>
#include <cstdio>

namespace gecko {

// Null service implementations
static NullJobSystem s_NullJobSystem;
static NullProfiler s_NullProfiler;
static NullLogger s_NullLogger;

// Service globals
static std::atomic<IAllocator*> g_Allocator {nullptr};
static std::atomic<IModuleRegistry*> g_Modules {nullptr};
static std::atomic<IEventBus*> g_EventBus {nullptr};
static std::atomic<IJobSystem*> g_JobSystem {&s_NullJobSystem};
static std::atomic<IProfiler*> g_Profiler {&s_NullProfiler};
static std::atomic<ILogger*> g_Logger {&s_NullLogger};
static std::atomic<bool> g_Installed {false};

struct ServiceInstallState
{
  bool allocator = false;
  bool jobSystem = false;
  bool profiler = false;
  bool logger = false;
  bool modules = false;
  bool eventBus = false;
};

static void RollbackServices(const Services& svc,
                             const ServiceInstallState& st) noexcept
{
  if (st.eventBus && svc.EventBus)
  {
    svc.EventBus->Shutdown();
    g_EventBus.store(nullptr, std::memory_order_release);
  }
  if (st.modules && svc.Modules)
  {
    svc.Modules->Shutdown();
    g_Modules.store(nullptr, std::memory_order_release);
  }
  if (st.logger && svc.Logger)
  {
    svc.Logger->Shutdown();
    g_Logger.store(&s_NullLogger, std::memory_order_release);
  }
  if (st.profiler && svc.Profiler)
  {
    svc.Profiler->Shutdown();
    g_Profiler.store(&s_NullProfiler, std::memory_order_release);
  }
  if (st.jobSystem && svc.JobSystem)
  {
    svc.JobSystem->Shutdown();
    g_JobSystem.store(&s_NullJobSystem, std::memory_order_release);
  }
  if (st.allocator && svc.Allocator)
  {
    svc.Allocator->Shutdown();
    g_Allocator.store(nullptr, std::memory_order_release);
  }
}

bool InstallServices(const Services& svc) noexcept
{
  if (g_Installed.load(std::memory_order_relaxed))
    return false;

  GECKO_ASSERT(svc.Allocator && "Allocator is required");
  GECKO_ASSERT(svc.Modules && "ModuleRegistry is required");
  GECKO_ASSERT(svc.EventBus && "EventBus is required");

  ServiceInstallState st {};

  // Level 0: Allocator
  if (!svc.Allocator->Init())
  {
    GECKO_ASSERT(false && "Allocator failed to initialize!");
    return false;
  }
  g_Allocator.store(svc.Allocator, std::memory_order_release);
  st.allocator = true;

  // Level 1: JobSystem
  if (svc.JobSystem)
  {
    if (!svc.JobSystem->Init())
    {
      RollbackServices(svc, st);
      return false;
    }
    g_JobSystem.store(svc.JobSystem, std::memory_order_release);
    st.jobSystem = true;
  }

  // Level 2: Profiler
  if (svc.Profiler)
  {
    if (!svc.Profiler->Init())
    {
      RollbackServices(svc, st);
      return false;
    }
    g_Profiler.store(svc.Profiler, std::memory_order_release);
    st.profiler = true;
  }

  // Level 3: Logger
  if (svc.Logger)
  {
    if (!svc.Logger->Init())
    {
      RollbackServices(svc, st);
      return false;
    }
    g_Logger.store(svc.Logger, std::memory_order_release);
    st.logger = true;
  }

  // Level 4: Modules
  if (!svc.Modules->Init())
  {
    RollbackServices(svc, st);
    return false;
  }
  g_Modules.store(svc.Modules, std::memory_order_release);
  st.modules = true;

  // Level 4: EventBus
  if (!svc.EventBus->Init())
  {
    RollbackServices(svc, st);
    return false;
  }
  g_EventBus.store(svc.EventBus, std::memory_order_release);
  st.eventBus = true;

  g_Installed.store(true, std::memory_order_release);

  GECKO_INFO(core::labels::Services, "All services installed successfully");

  return true;
}

void UninstallServices() noexcept
{
  GECKO_INFO(core::labels::Services, "Shutting down Gecko services");

  g_EventBus.load(std::memory_order_relaxed)->Shutdown();
  g_Modules.load(std::memory_order_relaxed)->Shutdown();
  g_Logger.load(std::memory_order_relaxed)->Shutdown();
  g_Profiler.load(std::memory_order_relaxed)->Shutdown();
  g_JobSystem.load(std::memory_order_relaxed)->Shutdown();
  g_Allocator.load(std::memory_order_relaxed)->Shutdown();

  g_Allocator.store(nullptr, std::memory_order_release);
  g_JobSystem.store(&s_NullJobSystem, std::memory_order_release);
  g_Profiler.store(&s_NullProfiler, std::memory_order_release);
  g_Logger.store(&s_NullLogger, std::memory_order_release);
  g_Modules.store(nullptr, std::memory_order_release);
  g_EventBus.store(nullptr, std::memory_order_release);
  g_Installed.store(false, std::memory_order_release);
}

IAllocator* GetAllocator() noexcept
{
  return g_Allocator.load(std::memory_order_acquire);
}

IJobSystem* GetJobSystem() noexcept
{
  return g_JobSystem.load(std::memory_order_acquire);
}

IProfiler* GetProfiler() noexcept
{
  return g_Profiler.load(std::memory_order_acquire);
}

ILogger* GetLogger() noexcept
{
  return g_Logger.load(std::memory_order_acquire);
}

IModuleRegistry* GetModules() noexcept
{
  return g_Modules.load(std::memory_order_acquire);
}

IEventBus* GetEventBus() noexcept
{
  return g_EventBus.load(std::memory_order_acquire);
}

bool IsServicesInstalled() noexcept
{
  return g_Installed.load(std::memory_order_acquire);
}

bool ValidateServices(bool fatalOnFail) noexcept
{
  bool ok = GetAllocator() && GetProfiler() && GetLogger() && GetJobSystem() &&
            GetModules() && IsServicesInstalled();

  if (!ok && fatalOnFail)
  {
    std::fputs("[Gecko] Services not properly installed.\n", stderr);
    GECKO_ASSERT(false);
  }

  return ok;
}

}  // namespace gecko
