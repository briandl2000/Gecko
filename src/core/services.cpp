#include "gecko/core/services.h"

#include "gecko/core/assert.h"
#include "gecko/core/services/log.h"
#include "gecko/core/services/profiler.h"
#include "private/labels.h"

#include <atomic>
#include <cstdio>

namespace gecko {

/*
 * SERVICE DEPENDENCY HIERARCHY
 * ============================
 *
 * The services are initialized and shut down in a specific order to avoid
 * circular dependencies:
 *
 * REQUIRED SERVICES (must be provided to InstallServices):
 * - Allocator    - Foundation for all memory allocation
 * - Modules      - Core architecture for organizing application code
 * - EventBus     - Core communication system between modules
 *
 * OPTIONAL SERVICES (will use null implementations if not provided):
 * - JobSystem    - Threading and background task processing
 * - Profiler     - Performance profiling and instrumentation
 * - Logger       - Logging system for diagnostics and debugging
 *
 * INITIALIZATION ORDER (low to high dependency):
 * Level 0: Allocator    - No dependencies on other services
 * Level 1: JobSystem    - May use allocator for job storage and worker threads
 * Level 2: Profiler     - May use allocator and job system for background
 * processing
 * Level 3: Logger       - May use allocator, job system, and profiler for
 * performance tracking
 * Level 4: Modules      - May use allocator, job system, profiler, and logger
 * Level 4: EventBus     - May use allocator and profiler
 *
 * SHUTDOWN ORDER (reverse of initialization):
 * EventBus -> Modules -> Logger -> Profiler -> JobSystem -> Allocator
 *
 * BENEFITS OF THIS ORDER:
 * - Allocator is always available as the foundation for all allocations
 * - JobSystem is available early for background processing by Profiler and
 * Logger
 * - Logger can use JobSystem for efficient background log processing
 * - Profiler can use JobSystem for background trace writing and data processing
 * - Modules and EventBus are the highest level organizational constructs
 */

// ------------------------------------------------

// Optional service null implementations
static NullJobSystem s_NullJobSystem;
static NullProfiler s_NullProfiler;
static NullLogger s_NullLogger;

// ----------------------------------------------------------------

// Required services start as nullptr - must be provided to InstallServices
static ::std::atomic<IAllocator*> g_Allocator {nullptr};
static ::std::atomic<IModuleRegistry*> g_Modules {nullptr};
static ::std::atomic<IEventBus*> g_EventBus {nullptr};

// Optional services have null implementations as fallback
static ::std::atomic<IJobSystem*> g_JobSystem {&s_NullJobSystem};
static ::std::atomic<IProfiler*> g_Profiler {&s_NullProfiler};
static ::std::atomic<ILogger*> g_Logger {&s_NullLogger};
static ::std::atomic<bool> g_Installed {false};

// Service installation helper - tracks which services have been installed
struct ServiceInstallState
{
  bool allocator = false;
  bool jobSystem = false;
  bool profiler = false;
  bool logger = false;
  bool modules = false;
  bool eventBus = false;
};

static void RollbackServices(const Services& service,
                             const ServiceInstallState& state) noexcept
{
  // Shutdown in reverse dependency order
  if (state.eventBus && service.EventBus)
  {
    service.EventBus->Shutdown();
    g_EventBus.store(nullptr, ::std::memory_order_release);
  }

  if (state.modules && service.Modules)
  {
    service.Modules->Shutdown();
    g_Modules.store(nullptr, ::std::memory_order_release);
  }

  if (state.logger && service.Logger)
  {
    service.Logger->Shutdown();
    g_Logger.store(&s_NullLogger, ::std::memory_order_release);
  }

  if (state.profiler && service.Profiler)
  {
    service.Profiler->Shutdown();
    g_Profiler.store(&s_NullProfiler, ::std::memory_order_release);
  }

  if (state.jobSystem && service.JobSystem)
  {
    service.JobSystem->Shutdown();
    g_JobSystem.store(&s_NullJobSystem, ::std::memory_order_release);
  }

  if (state.allocator && service.Allocator)
  {
    service.Allocator->Shutdown();
    g_Allocator.store(nullptr, ::std::memory_order_release);
  }
}

bool InstallServices(const Services& service) noexcept
{
  // NOTE: Cannot use profiling here - profiler is being initialized!

  if (g_Installed.load(::std::memory_order_relaxed))
  {
    return false;
  }

  // Validate required services are provided
  GECKO_ASSERT(service.Allocator &&
               "Allocator is required - must provide allocator to "
               "InstallServices");
  GECKO_ASSERT(service.Modules &&
               "ModuleRegistry is required - must provide module registry to "
               "InstallServices");
  GECKO_ASSERT(service.EventBus &&
               "EventBus is required - must provide event bus to "
               "InstallServices");

  ServiceInstallState state {};

  // Initialize services in dependency order
  // Each level can depend on previously initialized services

  // Level 0: Allocator (REQUIRED - no dependencies)
  if (!service.Allocator->Init())
  {
    GECKO_ASSERT(false && "Allocator failed to initialize!");
    RollbackServices(service, state);
    return false;
  }
  g_Allocator.store(service.Allocator, ::std::memory_order_release);
  state.allocator = true;

  // Level 1: JobSystem (may use allocator)
  if (service.JobSystem)
  {
    if (!service.JobSystem->Init())
    {
      GECKO_ASSERT(false && "JobSystem failed to initialize!");
      RollbackServices(service, state);
      return false;
    }
    g_JobSystem.store(service.JobSystem, ::std::memory_order_release);
    state.jobSystem = true;
  }

  // Level 2: Profiler (may use allocator and job system)
  if (service.Profiler)
  {
    if (!service.Profiler->Init())
    {
      GECKO_ASSERT(false && "Profiler failed to initialize!");
      RollbackServices(service, state);
      return false;
    }
    g_Profiler.store(service.Profiler, ::std::memory_order_release);
    state.profiler = true;
  }

  // Level 3: Logger (may use allocator, job system, and profiler)
  if (service.Logger)
  {
    if (!service.Logger->Init())
    {
      GECKO_ASSERT(false && "Logger failed to initialize!");
      RollbackServices(service, state);
      return false;
    }
    g_Logger.store(service.Logger, ::std::memory_order_release);
    state.logger = true;
  }

  // Level 4: Modules (REQUIRED - may use all previous services)
  if (!service.Modules->Init())
  {
    GECKO_ASSERT(false && "ModuleRegistry failed to initialize!");
    RollbackServices(service, state);
    return false;
  }
  g_Modules.store(service.Modules, ::std::memory_order_release);
  state.modules = true;

  // Level 4: EventBus (REQUIRED - may use allocator and profiler)
  if (!service.EventBus->Init())
  {
    GECKO_ASSERT(false && "EventBus failed to initialize!");
    RollbackServices(service, state);
    return false;
  }
  g_EventBus.store(service.EventBus, ::std::memory_order_release);
  state.eventBus = true;

  g_Installed.store(true, ::std::memory_order_release);

  // Now that logger is installed and ready, we can safely log
  GECKO_INFO(core::labels::Services, "All services installed successfully");

  return true;
}

void UninstallServices() noexcept
{
  GECKO_INFO(core::labels::Services, "Shutting down Gecko services");

  // Logger/Profiler wait for jobs, then JobSystem stops threads, then Allocator
  g_Logger.load(::std::memory_order_relaxed)->Shutdown();
  g_Profiler.load(::std::memory_order_relaxed)->Shutdown();
  g_JobSystem.load(::std::memory_order_relaxed)->Shutdown();
  g_Modules.load(::std::memory_order_relaxed)->Shutdown();
  g_EventBus.load(::std::memory_order_relaxed)->Shutdown();
  g_Allocator.load(::std::memory_order_relaxed)->Shutdown();

  // Reset to default implementations
  g_Allocator.store(nullptr, ::std::memory_order_release);
  g_JobSystem.store(&s_NullJobSystem, ::std::memory_order_release);
  g_Profiler.store(&s_NullProfiler, ::std::memory_order_release);
  g_Logger.store(&s_NullLogger, ::std::memory_order_release);
  g_Modules.store(nullptr, ::std::memory_order_release);
  g_EventBus.store(nullptr, ::std::memory_order_release);
  g_Installed.store(false, ::std::memory_order_release);
}

IAllocator* GetAllocator() noexcept
{
  auto* allocator = g_Allocator.load(::std::memory_order_acquire);
  GECKO_ASSERT(
      allocator &&
      "Allocator not available - services may not be properly installed");
  return allocator;
}

IJobSystem* GetJobSystem() noexcept
{
  auto* jobSystem = g_JobSystem.load(::std::memory_order_acquire);
  GECKO_ASSERT(
      jobSystem &&
      "JobSystem not available - services may not be properly installed");
  return jobSystem;
}

IProfiler* GetProfiler() noexcept
{
  auto* profiler = g_Profiler.load(::std::memory_order_acquire);
  GECKO_ASSERT(
      profiler &&
      "Profiler not available - services may not be properly installed");
  return profiler;
}

ILogger* GetLogger() noexcept
{
  auto* logger = g_Logger.load(::std::memory_order_acquire);
  GECKO_ASSERT(logger &&
               "Logger not available - services may not be properly installed");
  return logger;
}

IModuleRegistry* GetModules() noexcept
{
  auto* modules = g_Modules.load(::std::memory_order_acquire);
  GECKO_ASSERT(
      modules &&
      "Modules not available - services may not be properly installed");
  return modules;
}

IEventBus* GetEventBus() noexcept
{
  auto* eventBus = g_EventBus.load(::std::memory_order_acquire);
  GECKO_ASSERT(
      eventBus &&
      "EventBus not available - services may not be properly installed");
  return eventBus;
}

bool IsServicesInstalled() noexcept
{
  return g_Installed.load(::std::memory_order_acquire);
}

bool ValidateServices(bool fatalOnFail) noexcept
{
  bool ok = true;
  if (!GetAllocator())
    ok = false;
  if (!GetProfiler())
    ok = false;
  if (!GetLogger())
    ok = false;
  if (!GetJobSystem())
    ok = false;
  if (!GetModules())
    ok = false;

#if GECKO_REQUIRE_INSTALL
  if (!IsServicesInstalled())
    ok = false;
#endif

  if (!ok && fatalOnFail)
  {
    ::std::fputs("[Gecko] Services not properly installed. Call "
                 "gecko::InstallServices early in main().\n",
                 stderr);
    GECKO_ASSERT(false);
    return false;
  }

  return ok;
}
}  // namespace gecko
