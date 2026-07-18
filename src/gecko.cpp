#include "gecko/engine.h"

#include "core/private/services.h"
#include "gecko/core/services/memory.h"
#include "gecko/runtime/event_bus.h"
#include "gecko/runtime/immediate_logger.h"
#include "gecko/runtime/ring_profiler.h"
#include "gecko/runtime/standard_log_sinks.h"
#include "gecko/runtime/thread_pool_job_system.h"
#include "gecko/runtime/tracking_allocator.h"
#include "graphics/private/graphics_state.h"
#include "platform/private/platform_state.h"

#include <new>
#include <optional>

namespace gecko {

namespace {

struct GeckoState
{
  GeckoState() noexcept : AllocatorScope(Allocator), Profiler(1U << 16U)
  {
    Logger.SetThreadSafe(true);
  }

  runtime::TrackingAllocator Allocator;
  gecko::AllocatorScope AllocatorScope;
  runtime::ThreadPoolJobSystem Jobs;
  runtime::RingProfiler Profiler;
  runtime::ImmediateLogger Logger;
  runtime::EventBus Events;
  ::std::optional<runtime::StandardLogSinks> LogSinks;

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
  state.LogSinks.reset();

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

  detail::SetRuntimeServices(nullptr, nullptr, nullptr, nullptr);
}

}  // namespace

InitializeResult Initialize(const GeckoConfig& config) noexcept
{
  if (g_State != nullptr)
    return InitializeResult::AlreadyInitialized;

  void* memory = PlatformAlloc(sizeof(GeckoState), alignof(GeckoState));
  if (memory == nullptr)
    return InitializeResult::OutOfMemory;

  auto* state = new (memory) GeckoState();
  if (!state->AllocatorScope)
  {
    state->~GeckoState();
    PlatformFree(memory, alignof(GeckoState));
    return InitializeResult::RuntimeFailed;
  }

  if (!state->Jobs.Init())
    goto failed;
  state->JobsInitialized = true;
  detail::SetRuntimeServices(&state->Jobs, nullptr, nullptr, nullptr);

  if (!state->Profiler.Init())
    goto failed;
  state->ProfilerInitialized = true;
  detail::SetRuntimeServices(&state->Jobs, &state->Profiler, nullptr, nullptr);

  if (!state->Logger.Init())
    goto failed;
  state->LoggerInitialized = true;
  detail::SetRuntimeServices(&state->Jobs, &state->Profiler, &state->Logger, nullptr);

  if (!state->Events.Init())
    goto failed;
  state->EventsInitialized = true;
  detail::SetRuntimeServices(&state->Jobs, &state->Profiler, &state->Logger, &state->Events);

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

  state->LogSinks.emplace();
  g_State = state;
  return InitializeResult::Success;

failed:
  ShutdownState(*state);
  state->~GeckoState();
  PlatformFree(memory, alignof(GeckoState));
  return InitializeResult::RuntimeFailed;
}

void Shutdown() noexcept
{
  if (g_State == nullptr)
    return;

  GeckoState* state = g_State;
  g_State = nullptr;
  ShutdownState(*state);
  state->~GeckoState();
  PlatformFree(state, alignof(GeckoState));
}

bool IsInitialized() noexcept
{
  return g_State != nullptr;
}

}  // namespace gecko
