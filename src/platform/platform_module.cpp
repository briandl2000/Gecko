#include "gecko/platform/platform_module.h"

#include "gecko/core/scope.h"
#include "gecko/core/services/log.h"

#if defined(GECKO_PLATFORM_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
// needs to be after Windows.h to avoid macro conflicts (e.g. with CreateWindow).
#include <timeapi.h>
#endif

namespace gecko::platform {

static PlatformModule s_PlatformModule;

bool PlatformModule::Startup(::gecko::IModuleRegistry& /*modules*/) noexcept
{
  GECKO_FUNC(labels::Platform);

#if defined(GECKO_PLATFORM_WINDOWS)
  // Request 1ms timer resolution so sleep/timer functions are accurate.
  // Without this, Sleep(16) rounds up to ~31ms on default 15.6ms ticks.
  // Note: increases power usage slightly; restored in Shutdown.
  if (::timeBeginPeriod(1) != TIMERR_NOERROR)
    GECKO_WARN(labels::Platform, "timeBeginPeriod(1) failed");
#endif

  return true;
}

void PlatformModule::Shutdown(::gecko::IModuleRegistry& /*modules*/) noexcept
{
  GECKO_FUNC(labels::Platform);

#if defined(GECKO_PLATFORM_WINDOWS)
  ::timeEndPeriod(1);
#endif
}

::gecko::ModuleRegistration InstallPlatformModule(
    ::gecko::IModuleRegistry& modules) noexcept
{
  return modules.RegisterStatic(s_PlatformModule);
}

::gecko::IModule& GetModule() noexcept
{
  return s_PlatformModule;
}

}  // namespace gecko::platform
