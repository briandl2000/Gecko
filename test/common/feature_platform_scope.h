#pragma once

#include "gecko/platform/platform_config.h"
#include "gecko/platform/platform_context.h"
#include "test_service_scope.h"

#include <cstdlib>

namespace gecko::test {

/// Returns true if a live display server is available for the current
/// platform.  Feature tests that require a real backend should skip
/// when this returns false.
inline bool HasLiveDisplay() noexcept
{
#if defined(GECKO_PLATFORM_WINDOWS)
  // Win32 always has a display available when running interactively.
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
///     scope.Ctx.PumpEvents();
///     ...
///   }
struct FeaturePlatformScope
{
  TestServiceScope Services;
  platform::PlatformContext Ctx;

  FeaturePlatformScope() : Ctx(MakeConfig())
  {
    if (!HasLiveDisplay())
      SKIP("No live display server available");
  }

private:
  static platform::PlatformConfig MakeConfig() noexcept
  {
    platform::PlatformConfig cfg {};
    cfg.Backend = platform::DisplayBackendKind::Auto;
    return platform::Resolve(cfg);
  }
};

}  // namespace gecko::test
