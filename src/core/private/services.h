#pragma once

namespace gecko {

class IEventBus;
struct IJobSystem;
struct ILogger;
struct IProfiler;

namespace detail {

void SetRuntimeServices(IJobSystem* jobs, IProfiler* profiler, ILogger* logger, IEventBus* events) noexcept;

}  // namespace detail

}  // namespace gecko
