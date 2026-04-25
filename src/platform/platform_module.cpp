#include "gecko/platform/platform_module.h"

#include "gecko/core/scope.h"

#if defined(GECKO_PLATFORM_WINDOWS)
#include "gecko/core/services/log.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
// needs to be after Windows.h to avoid macro conflicts (e.g. with
// CreateWindow).
#include <timeapi.h>
#endif

namespace gecko::platform {

namespace {

// Services this module needs live during Startup/Shutdown.
// Topological sort uses these to start the publisher first.
constexpr ::gecko::ServiceId kRequired[] = {
    ::gecko::ServiceIdOf<::gecko::ILogger>(),
    ::gecko::ServiceIdOf<::gecko::IProfiler>(),
    ::gecko::ServiceIdOf<::gecko::IJobSystem>(),
    ::gecko::ServiceIdOf<::gecko::IEventBus>(),
};

}  // namespace

::std::span<const ::gecko::ServiceId> PlatformModule::Requires() const noexcept
{
  return ::std::span<const ::gecko::ServiceId> {kRequired};
}

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

}  // namespace gecko::platform
