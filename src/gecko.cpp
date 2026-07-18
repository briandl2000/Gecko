#include "gecko/core/placement.h"
#include "gecko/core/services/memory.h"
#include "gecko/core/utility/time.h"
#include "gecko/engine.h"
#include "graphics/private/graphics_state.h"
#include "platform/private/platform_state.h"
#include "runtime/private/event_bus.h"
#include "runtime/private/immediate_logger.h"
#include "runtime/private/ring_profiler.h"
#include "runtime/private/thread_pool_job_system.h"

namespace gecko {

namespace {

struct GeckoState
{
  GeckoState() noexcept : Profiler(1U << 16U)
  {}

  runtime::ThreadPoolJobSystem Jobs;
  runtime::RingProfiler Profiler;
  runtime::ImmediateLogger Logger;
  runtime::EventBus Events;

  bool JobsInitialized {false};
  bool ProfilerInitialized {false};
  bool LoggerInitialized {false};
  bool EventsInitialized {false};
  bool PlatformInitialized {false};
  bool GraphicsInitialized {false};
};

GeckoState* g_State = nullptr;

void ShutdownState(GeckoState& state) noexcept
{
  if (state.GraphicsInitialized)
  {
    graphics::detail::Shutdown();
    state.GraphicsInitialized = false;
  }
  if (state.PlatformInitialized)
  {
    platform::detail::Shutdown();
    state.PlatformInitialized = false;
  }
  if (state.EventsInitialized)
  {
    state.Events.Shutdown();
    state.EventsInitialized = false;
  }
  if (state.LoggerInitialized)
  {
    state.Logger.Shutdown();
    state.LoggerInitialized = false;
  }
  if (state.ProfilerInitialized)
  {
    state.Profiler.Shutdown();
    state.ProfilerInitialized = false;
  }
  if (state.JobsInitialized)
  {
    state.Jobs.Shutdown();
    state.JobsInitialized = false;
  }
}

}  // namespace

InitializeResult Initialize(const GeckoConfig& config) noexcept
{
  if (g_State != nullptr)
    return InitializeResult::AlreadyInitialized;

  void* memory = AllocBytes(sizeof(GeckoState), alignof(GeckoState));
  if (memory == nullptr)
    return InitializeResult::OutOfMemory;

  auto* state = new (memory, Placement) GeckoState();
  g_State = state;
  state->Jobs.SetWorkerThreadCount(config.JobWorkerCount);
  state->Logger.SetLevel(config.MinimumLogLevel);
  state->Profiler.SetMinLevel(config.ProfilerLevel);
  if (!state->Jobs.Init())
    goto failed;
  state->JobsInitialized = true;

  if (!state->Profiler.Init())
    goto failed;
  state->ProfilerInitialized = true;

  if (!state->Logger.Initialize())
    goto failed;
  state->LoggerInitialized = true;

  if (!state->Events.Init())
    goto failed;
  state->EventsInitialized = true;

  if (!platform::detail::Initialize(config.Platform))
    goto failed;
  state->PlatformInitialized = true;

  if (config.EnableGraphics)
  {
    const graphics::GraphicsConfig graphicsConfig {
        .Backend = config.GraphicsBackend,
        .Debug = config.EnableGraphicsDebug,
        .AppName = config.AppName,
    };
    if (!graphics::detail::Initialize(graphicsConfig))
      goto failed;
    state->GraphicsInitialized = true;
  }

  return InitializeResult::Success;

failed:
  ShutdownState(*state);
  g_State = nullptr;
  state->~GeckoState();
  DeallocBytes(memory);
  return InitializeResult::RuntimeFailed;
}

void Shutdown() noexcept
{
  if (g_State == nullptr)
    return;

  GeckoState* state = g_State;
  ShutdownState(*state);
  g_State = nullptr;
  state->~GeckoState();
  DeallocBytes(state);
}

bool IsInitialized() noexcept
{
  return g_State != nullptr;
}

void LogFormatted(LogLevel level, Label label, const char* format, Span<const FormatArg> arguments) noexcept
{
  if (g_State != nullptr && g_State->LoggerInitialized)
    g_State->Logger.LogFormatted(level, label, format, arguments);
}

void SetLogLevel(LogLevel level) noexcept
{
  if (g_State != nullptr)
    g_State->Logger.SetLevel(level);
}

LogLevel GetLogLevel() noexcept
{
  return g_State != nullptr ? g_State->Logger.GetLevel() : LogLevel::Info;
}

JobHandle SubmitRawJob(JobFn job, JobPriority priority, Label label) noexcept
{
  if (g_State != nullptr && g_State->JobsInitialized)
    return g_State->Jobs.SubmitRaw(job, priority, label);
  if (job.Free != nullptr && job.User != nullptr)
    job.Free(job.User);
  return {};
}

JobHandle SubmitRawJob(JobFn job, const JobHandle* dependencies, u32 dependencyCount, JobPriority priority,
                       Label label) noexcept
{
  if (g_State != nullptr && g_State->JobsInitialized)
    return g_State->Jobs.SubmitRaw(job, dependencies, dependencyCount, priority, label);
  if (job.Free != nullptr && job.User != nullptr)
    job.Free(job.User);
  return {};
}

void WaitForJob(JobHandle handle) noexcept
{
  if (g_State != nullptr && g_State->JobsInitialized)
    g_State->Jobs.Wait(handle);
}

void WaitForJobs(const JobHandle* handles, u32 count) noexcept
{
  if (g_State != nullptr && g_State->JobsInitialized)
    g_State->Jobs.WaitAll(handles, count);
}

bool IsJobComplete(JobHandle handle) noexcept
{
  return g_State == nullptr || !g_State->JobsInitialized || g_State->Jobs.IsComplete(handle);
}

u32 JobWorkerCount() noexcept
{
  return g_State != nullptr && g_State->JobsInitialized ? g_State->Jobs.WorkerThreadCount() : 0;
}

void ProcessJobs(u32 maxJobs) noexcept
{
  if (g_State != nullptr && g_State->JobsInitialized)
    g_State->Jobs.ProcessJobs(maxJobs);
}

void EmitProfileEvent(const ProfEvent& event) noexcept
{
  if (g_State != nullptr && g_State->ProfilerInitialized)
    g_State->Profiler.Emit(event);
}

u64 ProfilerNowNs() noexcept
{
  return MonotonicTimeNs();
}

void SetProfilerLevel(ProfLevel level) noexcept
{
  if (g_State != nullptr)
    g_State->Profiler.SetMinLevel(level);
}

ProfLevel GetProfilerLevel() noexcept
{
  return g_State != nullptr ? g_State->Profiler.GetMinLevel() : ProfLevel::Always;
}

bool IsProfilerLevelEnabled(ProfLevel level) noexcept
{
  return g_State != nullptr && g_State->ProfilerInitialized && g_State->Profiler.IsLevelEnabled(level);
}

ScopeStats GetScopeStats(u32 nameHash, ProfSource source) noexcept
{
  return g_State != nullptr ? g_State->Profiler.GetStats(nameHash, source) : ScopeStats {};
}

void ResetProfilerStats() noexcept
{
  if (g_State != nullptr)
    g_State->Profiler.ResetStats();
}

void ForEachProfileScope(ForEachScopeFn callback, void* user) noexcept
{
  if (g_State != nullptr)
    g_State->Profiler.ForEachScope(callback, user);
}

void DumpProfilerStats(Label label) noexcept
{
  if (g_State != nullptr)
    g_State->Profiler.DumpStats(label);
}

ProfilerDiagnostics GetProfilerDiagnostics() noexcept
{
  return g_State != nullptr ? g_State->Profiler.GetDiagnostics() : ProfilerDiagnostics {};
}

bool RegisterEventModule(u64 moduleId) noexcept
{
  return g_State != nullptr && g_State->EventsInitialized && g_State->Events.RegisterModule(moduleId);
}

void UnregisterEventModule(u64 moduleId) noexcept
{
  if (g_State != nullptr && g_State->EventsInitialized)
    g_State->Events.UnregisterModule(moduleId);
}

EventEmitter CreateEmitter(u64 moduleId, u64 sender) noexcept
{
  return g_State != nullptr && g_State->EventsInitialized ? g_State->Events.CreateEmitter(moduleId, sender)
                                                          : EventEmitter {};
}

EventEmitter CreateEmitterForModule(Label moduleLabel, u64 sender) noexcept
{
  if (!moduleLabel.IsValid())
    return {};
  return CreateEmitter(moduleLabel.Id, sender);
}

bool ValidateEmitter(const EventEmitter& emitter, u64 expectedModuleId) noexcept
{
  return g_State != nullptr && g_State->EventsInitialized && g_State->Events.ValidateEmitter(emitter, expectedModuleId);
}

EventSubscription SubscribeEvent(EventCode code, EventCallbackFn callback, void* user,
                                 SubscriptionOptions options) noexcept
{
  return g_State != nullptr && g_State->EventsInitialized ? g_State->Events.Subscribe(code, callback, user, options)
                                                          : EventSubscription {};
}

void SendEvent(const EventEmitter& emitter, EventCode code, EventView payload) noexcept
{
  if (g_State != nullptr && g_State->EventsInitialized)
    g_State->Events.Send(emitter, code, payload);
}

usize DispatchEvents(usize maxCount) noexcept
{
  return g_State != nullptr && g_State->EventsInitialized ? g_State->Events.Dispatch(maxCount) : 0;
}

void EventSubscription::Reset() noexcept
{
  if (m_Id != 0 && g_State != nullptr && g_State->EventsInitialized)
    g_State->Events.Unsubscribe(m_Id);
  m_Id = 0;
}

}  // namespace gecko
