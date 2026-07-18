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
#include "gecko/core/services/profiler.h"

namespace gecko {

/// @return Active job system. Never null.
GECKO_API IJobSystem* GetJobSystem() noexcept;
/// @return Active profiler. Never null.
GECKO_API IProfiler* GetProfiler() noexcept;
/// @return Active logger. Never null.
GECKO_API ILogger* GetLogger() noexcept;
/// @return Active event bus. Never null.
GECKO_API IEventBus* GetEventBus() noexcept;

}  // namespace gecko
