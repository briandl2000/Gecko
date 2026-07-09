#pragma once

#include "gecko/core/engine.h"
#include "gecko/core/services.h"
#include "gecko/platform/platform_config.h"
#include "gecko/platform/platform_module.h"
#include "gecko/runtime/event_bus.h"
#include "gecko/runtime/runtime_module.h"

#include <catch2/catch_test_macros.hpp>
#include <cstdlib>

namespace gecko::test {

inline bool HasLiveDisplay() noexcept
{
#if defined(GECKO_PLATFORM_WINDOWS)
  return true;
#elif defined(GECKO_PLATFORM_LINUX)
  const char* wayland = ::std::getenv("WAYLAND_DISPLAY");
  if (wayland && wayland[0] != '\0')
    return true;
  const char* x11 = ::std::getenv("DISPLAY");
  if (x11 && x11[0] != '\0')
    return true;
  return false;
#else
  return false;
#endif
}

/// RAII scope that boots real platform services using the auto-detected
/// backend.  Skips the calling test if no live display is available.
///
/// Usage:
///   TEST_CASE("...", "[feature][platform]") {
///     FeaturePlatformScope scope;
///     gecko::platform::PumpEvents();
///     auto* win = gecko::platform::GetWindows();
///   }
struct FeaturePlatformScope
{
  SystemAllocator Alloc;
  NullJobSystem Jobs;
  NullProfiler Profiler;
  NullLogger Logger;
  runtime::EventBus Events;
  runtime::RuntimeModule Runtime;
  platform::PlatformModule Platform;
  ::gecko::EngineResult EngineHandle;

  static platform::PlatformConfig MakeAutoConfig() noexcept
  {
    platform::PlatformConfig cfg;
    cfg.Backend = platform::DisplayBackendKind::Auto;
    return cfg;
  }

  FeaturePlatformScope() : Runtime(Jobs, Profiler, Logger, Events), Platform(MakeAutoConfig())
  {
    if (!HasLiveDisplay())
      SKIP("No live display server available");
    REQUIRE(SetAllocator(&Alloc));
    ::gecko::IModule* modules[] = {&Runtime, &Platform};
    EngineHandle = ::gecko::Engine::Create(modules);
    REQUIRE(EngineHandle.has_value());
  }

  ~FeaturePlatformScope()
  {
    EngineHandle.reset();
    ResetAllocator();
  }

  FeaturePlatformScope(const FeaturePlatformScope&) = delete;
  FeaturePlatformScope& operator=(const FeaturePlatformScope&) = delete;
};

}  // namespace gecko::test
