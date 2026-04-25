#include "gecko/platform/platform_module.h"

#include "gecko/core/scope.h"
#include "gecko/core/services/log.h"

#if defined(GECKO_PLATFORM_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
// after Windows.h to avoid macro conflicts.
#include <timeapi.h>
#endif

namespace gecko::platform {

namespace {

constexpr ::gecko::ServiceId kRequired[] = {
    ::gecko::ServiceIdOf<::gecko::ILogger>(),
    ::gecko::ServiceIdOf<::gecko::IProfiler>(),
    ::gecko::ServiceIdOf<::gecko::IJobSystem>(),
    ::gecko::ServiceIdOf<::gecko::IEventBus>(),
};

}  // namespace

PlatformModule::PlatformModule() noexcept = default;
PlatformModule::~PlatformModule() noexcept = default;

::std::span<const ::gecko::ServiceId> PlatformModule::Requires() const noexcept
{
  return ::std::span<const ::gecko::ServiceId> {kRequired};
}

bool PlatformModule::Startup(::gecko::IModuleRegistry& /*modules*/) noexcept
{
  GECKO_FUNC(labels::Platform);

#if defined(GECKO_PLATFORM_WINDOWS)
  // Request 1ms timer resolution so SleepNanoseconds is accurate.
  // Without this, Sleep(16) rounds up to ~31ms on default 15.6ms ticks.
  // Restored in Shutdown.
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
