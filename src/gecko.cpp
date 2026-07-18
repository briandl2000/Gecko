#include "core/private/services.h"
#include "gecko/core/services/memory.h"
#include "gecko/engine.h"
#include "gecko/runtime/event_bus.h"
#include "gecko/runtime/immediate_logger.h"
#include "gecko/runtime/ring_profiler.h"
#include "gecko/runtime/thread_pool_job_system.h"
#include "graphics/private/graphics_state.h"
#include "platform/private/platform_state.h"

#include <new>

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

  detail::SetRuntimeServices(nullptr, nullptr, nullptr, nullptr);
}

}  // namespace

InitializeResult Initialize(const GeckoConfig& config) noexcept
{
  if (g_State != nullptr)
    return InitializeResult::AlreadyInitialized;

  void* memory = AllocBytes(sizeof(GeckoState), alignof(GeckoState));
  if (memory == nullptr)
    return InitializeResult::OutOfMemory;

  auto* state = new (memory) GeckoState();
  if (!state->Jobs.Init())
    goto failed;
  state->JobsInitialized = true;
  detail::SetRuntimeServices(&state->Jobs, nullptr, nullptr, nullptr);

  if (!state->Profiler.Init())
    goto failed;
  state->ProfilerInitialized = true;
  detail::SetRuntimeServices(&state->Jobs, &state->Profiler, nullptr, nullptr);

  if (!state->Logger.Initialize())
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

  g_State = state;
  return InitializeResult::Success;

failed:
  ShutdownState(*state);
  state->~GeckoState();
  DeallocBytes(memory);
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
  DeallocBytes(state);
}

bool IsInitialized() noexcept
{
  return g_State != nullptr;
}

}  // namespace gecko
