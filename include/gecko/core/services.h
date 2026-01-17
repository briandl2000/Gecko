#pragma once

#include "gecko/core/api.h"
#include "gecko/core/services/events.h"
#include "gecko/core/services/jobs.h"
#include "gecko/core/services/log.h"
#include "gecko/core/services/memory.h"
#include "gecko/core/services/modules.h"
#include "gecko/core/services/profiler.h"

namespace gecko {

struct Services
{
  IAllocator* Allocator = nullptr;
  IJobSystem* JobSystem = nullptr;
  IProfiler* Profiler = nullptr;
  ILogger* Logger = nullptr;
  IModuleRegistry* Modules = nullptr;
  IEventBus* EventBus = nullptr;
};

[[nodiscard]]
GECKO_API bool InstallServices(const Services& service) noexcept;

GECKO_API void UninstallServices() noexcept;

GECKO_API IAllocator* GetAllocator() noexcept;
GECKO_API IJobSystem* GetJobSystem() noexcept;
GECKO_API IProfiler* GetProfiler() noexcept;
GECKO_API ILogger* GetLogger() noexcept;
GECKO_API IModuleRegistry* GetModules() noexcept;
GECKO_API IEventBus* GetEventBus() noexcept;

GECKO_API bool IsServicesInstalled() noexcept;

GECKO_API bool ValidateServices(bool fatalOnFail) noexcept;

}  // namespace gecko
#ifndef GECKO_REQUIRE_INSTALL
#define GECKO_REQUIRE_INSTALL 1
#endif
