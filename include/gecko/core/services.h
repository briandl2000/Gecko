#pragma once

#include "gecko/core/api.h"
#include "gecko/core/services/events.h"
#include "gecko/core/services/jobs.h"
#include "gecko/core/services/log.h"
#include "gecko/core/services/memory.h"
#include "gecko/core/services/modules.h"
#include "gecko/core/services/profiler.h"

namespace gecko {

// Service accessors. Each returns the implementation published by the
// active module registry, or a Null fallback if the engine has not yet
// booted (or the service was never published). They never return null.
GECKO_API IJobSystem* GetJobSystem() noexcept;
GECKO_API IProfiler* GetProfiler() noexcept;
GECKO_API ILogger* GetLogger() noexcept;
GECKO_API IModuleRegistry* GetModules() noexcept;
GECKO_API IEventBus* GetEventBus() noexcept;

namespace detail {

// Engine-only: install/uninstall the active module registry. User code
// must not call these — use Engine::Create() instead.
GECKO_API void SetActiveModuleRegistry(IModuleRegistry* registry) noexcept;

}  // namespace detail

}  // namespace gecko
