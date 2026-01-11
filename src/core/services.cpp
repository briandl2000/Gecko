#include "gecko/core/services.h"

#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstdlib>

#include "gecko/core/assert.h"
#include "gecko/core/log.h"

namespace gecko {

/*
 * SERVICE DEPENDENCY HIERARCHY
 * ============================
 *
 * The services are initialized and shut down in a specific order to avoid
 * circular dependencies:
 *
 * INITIALIZATION ORDER (low to high dependency):
 * Level 0: Allocator    - No dependencies on other services
 * Level 1: JobSystem    - May use allocator for job storage and worker threads
 * Level 2: Profiler     - May use allocator and job system for background
 * processing
 * Level 3: Logger       - May use allocator, job system, and profiler for
 * performance tracking
 * Level 4: Modules      - May use allocator, job system, profiler, and logger
 *
 * SHUTDOWN ORDER (reverse of initialization):
 * Level 4: Modules -> Level 3: Logger -> Level 2: Profiler -> Level 1:
 * JobSystem -> Level 0: Allocator
 *
 * BENEFITS OF THIS ORDER:
 * - JobSystem is available early for background processing by Profiler and
 * Logger
 * - Logger can use JobSystem for efficient background log processing
 * - Profiler can use JobSystem for background trace writing and data processing
 * - All services can use the Allocator as the foundation
 */

#pragma region(minimal defaults) // ------------------------------------------------

bool NullModuleRegistry::Init() noexcept { return true; }
void NullModuleRegistry::Shutdown() noexcept {}

ModuleRegistration
NullModuleRegistry::RegisterStatic(IModule &module) noexcept {
  const Label id = module.RootLabel();
  if (!id.IsValid()) {
    return ModuleRegistration{ModuleHandle{}, ModuleResult::InvalidArgument};
  }
  return ModuleRegistration{MakeHandle(id), ModuleResult::Ok};
}

ModuleResult NullModuleRegistry::Unregister(Label id) noexcept {
  (void)id;
  return ModuleResult::Ok;
}

IModule *NullModuleRegistry::GetModule(Label id) noexcept {
  (void)id;
  return nullptr;
}

const IModule *NullModuleRegistry::GetModule(Label id) const noexcept {
  (void)id;
  return nullptr;
}

void NullModuleRegistry::ForEachModule(ModuleVisitFn fn, void *user) noexcept {
  (void)fn;
  (void)user;
}

bool NullModuleRegistry::StartupAllModules() noexcept { return true; }
void NullModuleRegistry::ShutdownAllModules() noexcept {}

void *SystemAllocator::Alloc(u64 size, u32 alignment, Label label) noexcept {
  GECKO_ASSERT(size > 0 && "Cannot allocate zero bytes");
  GECKO_ASSERT(alignment > 0 && (alignment & (alignment - 1)) == 0 &&
               "Alignment must be power of 2");

  (void)label;

  if (alignment <= alignof(std::max_align_t)) {
    return std::malloc(static_cast<size_t>(size));
  }

#if defined(_MSC_VER)
  return _aligned_malloc(static_cast<size_t>(size), alignment);
#else
  void *ptr = nullptr;
  if (posix_memalign(&ptr, alignment, static_cast<size_t>(size)) != 0)
    return nullptr;
  return ptr;
#endif
}

void SystemAllocator::Free(void *ptr, u64 size, u32 alignment,
                           Label label) noexcept {
  if (!ptr)
    return;

  (void)size;
  (void)label;
#if defined(_MSC_VER)
  if (alignment > alignof(std::max_align_t)) {
    _aligned_free(ptr);
    return;
  }
#endif
  std::free(ptr);
}
bool SystemAllocator::Init() noexcept { return true; }
void SystemAllocator::Shutdown() noexcept {}

void NullProfiler::Emit(const ProfEvent &event) noexcept {}
u64 NullProfiler::NowNs() const noexcept { return 0; }
bool NullProfiler::Init() noexcept { return true; }
void NullProfiler::Shutdown() noexcept {}

void NullLogger::LogV(LogLevel level, Label label, const char *fmt,
                      va_list) noexcept {}
void NullLogger::AddSink(ILogSink *sink) noexcept {}
void NullLogger::SetLevel(LogLevel level) noexcept {}
LogLevel NullLogger::Level() const noexcept { return ::gecko::LogLevel::Info; }
void NullLogger::Flush() noexcept {}
bool NullLogger::Init() noexcept { return true; }
void NullLogger::Shutdown() noexcept {}

JobHandle NullJobSystem::Submit(JobFunction job, JobPriority priority,
                                Label label) noexcept {
  (void)priority;
  (void)label;
  // Execute job immediately in null implementation
  if (job)
    job();
  return JobHandle{};
}

JobHandle NullJobSystem::Submit(JobFunction job, const JobHandle *dependencies,
                                u32 dependencyCount, JobPriority priority,
                                Label label) noexcept {
  (void)dependencies;
  (void)dependencyCount;
  (void)priority;
  (void)label;
  // Execute job immediately in null implementation (dependencies are ignored)
  if (job)
    job();
  return JobHandle{};
}

void NullJobSystem::Wait(JobHandle handle) noexcept {}
void NullJobSystem::WaitAll(const JobHandle *handles, u32 count) noexcept {}
bool NullJobSystem::IsComplete(JobHandle handle) noexcept { return true; }
u32 NullJobSystem::WorkerThreadCount() const noexcept { return 0; }
void NullJobSystem::ProcessJobs(u32 maxJobs) noexcept {}
bool NullJobSystem::Init() noexcept { return true; }
void NullJobSystem::Shutdown() noexcept {}

static SystemAllocator s_SystemAllocator;
static NullJobSystem s_NullJobSystem;
static NullProfiler s_NullProfiler;
static NullLogger s_NullLogger;
static NullModuleRegistry s_NullModules;

#pragma endregion //----------------------------------------------------------------

static std::atomic<IAllocator *> g_Allocator{&s_SystemAllocator};
static std::atomic<IJobSystem *> g_JobSystem{&s_NullJobSystem};
static std::atomic<IProfiler *> g_Profiler{&s_NullProfiler};
static std::atomic<ILogger *> g_Logger{&s_NullLogger};
static std::atomic<IModuleRegistry *> g_Modules{&s_NullModules};
static std::atomic<bool> g_Installed{false};

bool InstallServices(const Services &service) noexcept {
  if (g_Installed.load(std::memory_order_relaxed)) {
    return false;
  }

  // Initialize services in dependency order: Allocator -> JobSystem -> Profiler
  // -> Logger Each level can depend on previously initialized services, but not
  // on later ones

  // Level 0: Allocator (no dependencies)
  if (service.Allocator) {
    if (!service.Allocator->Init()) {
      GECKO_ASSERT(false && "Allocator failed to initialize!");
      return false;
    }
    g_Allocator.store(service.Allocator, std::memory_order_release);
  }

  // Level 1: JobSystem (may use allocator for job storage and worker threads)
  if (service.JobSystem) {
    if (!service.JobSystem->Init()) {
      GECKO_ASSERT(false && "JobSystem failed to initialize!");
      // Rollback allocator if needed
      if (service.Allocator) {
        service.Allocator->Shutdown();
        g_Allocator.store(&s_SystemAllocator, std::memory_order_release);
      }
      return false;
    }
    g_JobSystem.store(service.JobSystem, std::memory_order_release);
  }

  // Level 2: Profiler (may use allocator and job system for background
  // processing)
  if (service.Profiler) {
    if (!service.Profiler->Init()) {
      GECKO_ASSERT(false && "Profiler failed to initialize!");
      // Rollback in reverse order
      if (service.JobSystem) {
        service.JobSystem->Shutdown();
        g_JobSystem.store(&s_NullJobSystem, std::memory_order_release);
      }
      if (service.Allocator) {
        service.Allocator->Shutdown();
        g_Allocator.store(&s_SystemAllocator, std::memory_order_release);
      }
      return false;
    }
    g_Profiler.store(service.Profiler, std::memory_order_release);
  }

  // Level 3: Logger (may use allocator, job system, and profiler for
  // performance tracking)
  if (service.Logger) {
    if (!service.Logger->Init()) {
      GECKO_ASSERT(false && "Logger failed to initialize!");
      // Rollback all previous services in reverse order
      if (service.Profiler) {
        service.Profiler->Shutdown();
        g_Profiler.store(&s_NullProfiler, std::memory_order_release);
      }
      if (service.JobSystem) {
        service.JobSystem->Shutdown();
        g_JobSystem.store(&s_NullJobSystem, std::memory_order_release);
      }
      if (service.Allocator) {
        service.Allocator->Shutdown();
        g_Allocator.store(&s_SystemAllocator, std::memory_order_release);
      }
      return false;
    }
    g_Logger.store(service.Logger, std::memory_order_release);
  }

  // Level 4: Modules (may use allocator, job system, profiler, logger)
  if (service.Modules) {
    if (!service.Modules->Init()) {
      GECKO_ASSERT(false && "ModuleRegistry failed to initialize!");
      // Rollback previous services in reverse order
      if (service.Logger) {
        service.Logger->Shutdown();
        g_Logger.store(&s_NullLogger, std::memory_order_release);
      }
      if (service.Profiler) {
        service.Profiler->Shutdown();
        g_Profiler.store(&s_NullProfiler, std::memory_order_release);
      }
      if (service.JobSystem) {
        service.JobSystem->Shutdown();
        g_JobSystem.store(&s_NullJobSystem, std::memory_order_release);
      }
      if (service.Allocator) {
        service.Allocator->Shutdown();
        g_Allocator.store(&s_SystemAllocator, std::memory_order_release);
      }
      return false;
    }
    g_Modules.store(service.Modules, std::memory_order_release);
  }

  g_Installed.store(true, std::memory_order_release);

  return true;
}

void UninstallServices() noexcept {
  // Shutdown services in reverse dependency order.
  // Modules shut down first so modules can still log during shutdown.

  static constexpr auto c_Services = ::gecko::MakeLabel("gecko.core.services");

  auto *modules = g_Modules.load(std::memory_order_relaxed);
  if (modules) {
    GECKO_INFO(c_Services, "UninstallServices: shutting down modules");
    modules->Shutdown();
    GECKO_INFO(c_Services, "UninstallServices: modules shutdown complete");
  }

  g_Logger.load(std::memory_order_relaxed)
      ->Shutdown(); // Level 3: May use profiler, job system, allocator
  g_Profiler.load(std::memory_order_relaxed)
      ->Shutdown(); // Level 2: May use job system, allocator
  g_JobSystem.load(std::memory_order_relaxed)
      ->Shutdown(); // Level 1: May use allocator
  g_Allocator.load(std::memory_order_relaxed)
      ->Shutdown(); // Level 0: No dependencies

  // Reset to default null implementations
  g_Allocator.store(&s_SystemAllocator, std::memory_order_release);
  g_JobSystem.store(&s_NullJobSystem, std::memory_order_release);
  g_Profiler.store(&s_NullProfiler, std::memory_order_release);
  g_Logger.store(&s_NullLogger, std::memory_order_release);
  g_Modules.store(&s_NullModules, std::memory_order_release);
  g_Installed.store(false, std::memory_order_release);
}

IAllocator *GetAllocator() noexcept {
  auto *allocator = g_Allocator.load(std::memory_order_acquire);
  GECKO_ASSERT(
      allocator &&
      "Allocator not available - services may not be properly installed");
  return allocator;
}

IJobSystem *GetJobSystem() noexcept {
  auto *jobSystem = g_JobSystem.load(std::memory_order_acquire);
  GECKO_ASSERT(
      jobSystem &&
      "JobSystem not available - services may not be properly installed");
  return jobSystem;
}

IProfiler *GetProfiler() noexcept {
  auto *profiler = g_Profiler.load(std::memory_order_acquire);
  GECKO_ASSERT(
      profiler &&
      "Profiler not available - services may not be properly installed");
  return profiler;
}

ILogger *GetLogger() noexcept {
  auto *logger = g_Logger.load(std::memory_order_acquire);
  GECKO_ASSERT(logger &&
               "Logger not available - services may not be properly installed");
  return logger;
}

IModuleRegistry *GetModules() noexcept {
  auto *modules = g_Modules.load(std::memory_order_acquire);
  GECKO_ASSERT(
      modules &&
      "Modules not available - services may not be properly installed");
  return modules;
}

bool IsServicesInstalled() noexcept {
  return g_Installed.load(std::memory_order_acquire);
}

bool ValidateServices(bool fatalOnFail) noexcept {
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

  if (!ok && fatalOnFail) {
    std::fputs("[Gecko] Services not properly installed. Call "
               "gecko::InstallServices early in main().\n",
               stderr);
    GECKO_ASSERT(false);
    return false;
  }

  return ok;
}
} // namespace gecko
