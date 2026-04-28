#pragma once

/// @file
/// Free-function accessors for the active service implementations.
///
/// These accessors look up services on the *active module registry*
/// (set by `Engine::Create`). When the engine has not yet booted, or
/// when a particular service has not been published, the accessors
/// return a process-wide `Null*` fallback so call sites never need to
/// null-check the pointer.

#include "gecko/core/api.h"
#include "gecko/core/services/events.h"
#include "gecko/core/services/jobs.h"
#include "gecko/core/services/log.h"
#include "gecko/core/services/memory.h"
#include "gecko/core/services/modules.h"
#include "gecko/core/services/profiler.h"

namespace gecko {

/// @return Active job system. Never null.
GECKO_API IJobSystem* GetJobSystem() noexcept;
/// @return Active profiler. Never null.
GECKO_API IProfiler* GetProfiler() noexcept;
/// @return Active logger. Never null.
GECKO_API ILogger* GetLogger() noexcept;
/// @return Active module registry. Never null.
GECKO_API IModuleRegistry* GetModules() noexcept;
/// @return Active event bus. Never null.
GECKO_API IEventBus* GetEventBus() noexcept;

namespace detail {

/// Engine-internal: install or clear the active module registry.
///
/// User code must not call this directly -- use `Engine::Create`, which
/// owns the registry lifetime and calls this on boot/teardown.
GECKO_API void SetActiveModuleRegistry(IModuleRegistry* registry) noexcept;

}  // namespace detail

}  // namespace gecko
