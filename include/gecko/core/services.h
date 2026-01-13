#pragma once

#include "api.h"
#include "events.h"
#include "jobs.h"
#include "log.h"
#include "memory.h"
#include "modules.h"
#include "profiler.h"

namespace gecko {

struct Services {
  IAllocator *Allocator = nullptr;
  IJobSystem *JobSystem = nullptr;
  IProfiler *Profiler = nullptr;
  ILogger *Logger = nullptr;
  IModuleRegistry *Modules = nullptr;
  IEventBus *EventBus = nullptr;
};

[[nodiscard]]
GECKO_API bool InstallServices(const Services &service) noexcept;

GECKO_API void UninstallServices() noexcept;

GECKO_API IAllocator *GetAllocator() noexcept;
GECKO_API IJobSystem *GetJobSystem() noexcept;
GECKO_API IProfiler *GetProfiler() noexcept;
GECKO_API ILogger *GetLogger() noexcept;
GECKO_API IModuleRegistry *GetModules() noexcept;
GECKO_API IEventBus *GetEventBus() noexcept;

GECKO_API bool IsServicesInstalled() noexcept;

GECKO_API bool ValidateServices(bool fatalOnFail) noexcept;

// Default service structs (in dependency order)

// Level 0: Allocator (no dependencies)
struct SystemAllocator final : IAllocator {
  GECKO_API virtual void *Alloc(u64 size, u32 alignment,
                                Label label) noexcept override;

  GECKO_API virtual void Free(void *ptr, u64 size, u32 alignment,
                              Label label) noexcept override;

  GECKO_API virtual bool Init() noexcept override;
  GECKO_API virtual void Shutdown() noexcept override;
};

// Level 1: JobSystem (may use allocator)
struct NullJobSystem final : IJobSystem {
  GECKO_API virtual JobHandle Submit(JobFunction job,
                                     JobPriority priority = JobPriority::Normal,
                                     Label label = Label{}) noexcept override;
  GECKO_API virtual JobHandle Submit(JobFunction job,
                                     const JobHandle *dependencies,
                                     u32 dependencyCount,
                                     JobPriority priority = JobPriority::Normal,
                                     Label label = Label{}) noexcept override;
  GECKO_API virtual void Wait(JobHandle handle) noexcept override;
  GECKO_API virtual void WaitAll(const JobHandle *handles,
                                 u32 count) noexcept override;
  GECKO_API virtual bool IsComplete(JobHandle handle) noexcept override;
  GECKO_API virtual u32 WorkerThreadCount() const noexcept override;
  GECKO_API virtual void ProcessJobs(u32 maxJobs = 1) noexcept override;

  GECKO_API virtual bool Init() noexcept override;
  GECKO_API virtual void Shutdown() noexcept override;
};

// Level 2: Profiler (may use allocator and job system)
struct NullProfiler final : IProfiler {
  GECKO_API virtual void Emit(const ProfEvent &event) noexcept override;
  GECKO_API virtual u64 NowNs() const noexcept override;

  GECKO_API virtual bool Init() noexcept override;
  GECKO_API virtual void Shutdown() noexcept override;
};

// Level 3: Logger (may use allocator, job system, and profiler)
struct NullLogger final : ILogger {
  GECKO_API virtual void LogV(LogLevel level, Label label, const char *fmt,
                              va_list) noexcept override;
  GECKO_API virtual void AddSink(ILogSink *sink) noexcept override;
  GECKO_API virtual void SetLevel(LogLevel level) noexcept override;
  GECKO_API virtual LogLevel Level() const noexcept override;

  GECKO_API virtual void Flush() noexcept override;

  GECKO_API virtual bool Init() noexcept override;
  GECKO_API virtual void Shutdown() noexcept override;
};

// Level 4: Modules (may use allocator, job system, profiler, and logger)
struct NullModuleRegistry final : IModuleRegistry {
  [[nodiscard]] GECKO_API virtual bool Init() noexcept override;
  GECKO_API virtual void Shutdown() noexcept override;

  [[nodiscard]] GECKO_API virtual ModuleRegistration
  RegisterStatic(IModule &module) noexcept override;
  [[nodiscard]] GECKO_API virtual ModuleResult
  Unregister(Label module) noexcept override;

  [[nodiscard]] GECKO_API virtual IModule *
  GetModule(Label module) noexcept override;
  [[nodiscard]] GECKO_API virtual const IModule *
  GetModule(Label module) const noexcept override;

  GECKO_API virtual void ForEachModule(ModuleVisitFn fn,
                                       void *user) noexcept override;

  [[nodiscard]] GECKO_API virtual bool StartupAllModules() noexcept override;
  GECKO_API virtual void ShutdownAllModules() noexcept override;
};

// Level 4: EventBus (may use allocator and profiler)
// NullEventBus: True no-op event bus (like NullLogger, NullProfiler)
// All events are silently discarded. Use runtime::EventBus for working events.
struct NullEventBus final : IEventBus {
  GECKO_API virtual EventSubscription
  Subscribe(EventCode code, CallbackFn fn, void *user,
            SubscriptionOptions options = {}) noexcept override;
  GECKO_API virtual void PublishImmediate(const EventEmitter &emitter,
                                          EventCode code,
                                          EventView payload) noexcept override;
  GECKO_API virtual void Enqueue(const EventEmitter &emitter, EventCode code,
                                 EventView payload) noexcept override;
  GECKO_API virtual std::size_t
  DispatchQueued(std::size_t maxCount) noexcept override;

  GECKO_API virtual bool RegisterModule(u64 moduleId) noexcept override;
  GECKO_API virtual void UnregisterModule(u64 moduleId) noexcept override;
  GECKO_API virtual EventEmitter CreateEmitter(u64 moduleId,
                                               u64 sender) noexcept override;
  GECKO_API virtual bool
  ValidateEmitter(const EventEmitter &emitter,
                  u64 expectedModuleId) const noexcept override;

  GECKO_API virtual bool Init() noexcept override;
  GECKO_API virtual void Shutdown() noexcept override;

protected:
  GECKO_API virtual void Unsubscribe(u64 id) noexcept override;
};

} // namespace gecko
#ifndef GECKO_REQUIRE_INSTALL
#define GECKO_REQUIRE_INSTALL 1
#endif
