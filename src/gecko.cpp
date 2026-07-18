#include "gecko/gecko.h"

#include "gecko/core/engine.h"
#include "gecko/core/services/memory.h"
#include "gecko/platform/platform_module.h"
#include "gecko/runtime/runtime_module.h"
#include "gecko/runtime/standard_log_sinks.h"
#include "gecko/runtime/tracking_allocator.h"

#include <new>
#include <optional>

namespace gecko {

namespace {

struct GeckoState
{
  explicit GeckoState(const GeckoConfig& config) noexcept
      : AllocatorScope(Allocator), Platform(config.Platform), Graphics(graphics::GraphicsConfig {
                                                                  .Backend = config.GraphicsBackend,
                                                                  .Debug = config.EnableGraphicsDebug,
                                                                  .AppName = config.AppName,
                                                              }),
        GraphicsEnabled(config.EnableGraphics)
  {}

  runtime::TrackingAllocator Allocator;
  gecko::AllocatorScope AllocatorScope;
  runtime::RuntimeModule Runtime;
  platform::PlatformModule Platform;
  graphics::GraphicsModule Graphics;
  bool GraphicsEnabled {true};

  ::std::optional<Engine> Lifecycle;
  ::std::optional<runtime::StandardLogSinks> LogSinks;
};

GeckoState* g_State = nullptr;

}  // namespace

InitializeResult Initialize(const GeckoConfig& config) noexcept
{
  if (g_State != nullptr)
    return InitializeResult::AlreadyInitialized;

  void* memory = PlatformAlloc(sizeof(GeckoState), alignof(GeckoState));
  if (memory == nullptr)
    return InitializeResult::OutOfMemory;

  auto* state = new (memory) GeckoState(config);
  if (!state->AllocatorScope)
  {
    state->~GeckoState();
    PlatformFree(memory, alignof(GeckoState));
    return InitializeResult::RuntimeFailed;
  }

  if (state->GraphicsEnabled)
    state->Lifecycle = Engine::Create({&state->Runtime, &state->Platform, &state->Graphics});
  else
    state->Lifecycle = Engine::Create({&state->Runtime, &state->Platform});

  if (!state->Lifecycle)
  {
    state->~GeckoState();
    PlatformFree(memory, alignof(GeckoState));
    return InitializeResult::RuntimeFailed;
  }

  state->LogSinks.emplace();
  g_State = state;
  return InitializeResult::Success;
}

void Shutdown() noexcept
{
  if (g_State == nullptr)
    return;

  GeckoState* state = g_State;
  g_State = nullptr;
  state->~GeckoState();
  PlatformFree(state, alignof(GeckoState));
}

bool IsInitialized() noexcept
{
  return g_State != nullptr;
}

}  // namespace gecko
