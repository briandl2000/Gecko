#include "gecko/platform/platform_module.h"

#include "gecko/core/scope.h"
#include "gecko/core/services/log.h"
#include "gecko/platform/threading.h"
#include "private/native_threading.h"

#if defined(GECKO_PLATFORM_WINDOWS)
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

// Services this module needs live during Startup/Shutdown. Topological
// sort uses these to start the services-publisher first.
constexpr ::gecko::ServiceId kRequired[] = {
    ::gecko::ServiceIdOf<::gecko::ILogger>(),
    ::gecko::ServiceIdOf<::gecko::IProfiler>(),
    ::gecko::ServiceIdOf<::gecko::IJobSystem>(),
    ::gecko::ServiceIdOf<::gecko::IEventBus>(),
};

// Services this module publishes during Startup. Other modules can
// declare these in their own Requires() to enforce ordering.
constexpr ::gecko::ServiceId kPublished[] = {
    ::gecko::ServiceIdOf<IThreading>(),
};

}  // namespace

PlatformModule::PlatformModule() noexcept = default;
PlatformModule::~PlatformModule() noexcept = default;

::std::span<const ::gecko::ServiceId> PlatformModule::Requires() const noexcept
{
  return ::std::span<const ::gecko::ServiceId> {kRequired};
}

::std::span<const ::gecko::ServiceId> PlatformModule::Publishes() const noexcept
{
  return ::std::span<const ::gecko::ServiceId> {kPublished};
}

bool PlatformModule::Startup(::gecko::IModuleRegistry& modules) noexcept
{
  GECKO_FUNC(labels::Platform);

#if defined(GECKO_PLATFORM_WINDOWS)
  // Request 1ms timer resolution so sleep/timer functions are accurate.
  // Without this, Sleep(16) rounds up to ~31ms on default 15.6ms ticks.
  // Note: increases power usage slightly; restored in Shutdown.
  if (::timeBeginPeriod(1) != TIMERR_NOERROR)
    GECKO_WARN(labels::Platform, "timeBeginPeriod(1) failed");
#endif

  m_Threading = CreateNativeThreading();
  if (m_Threading == nullptr)
  {
    GECKO_ERROR(labels::Platform, "CreateNativeThreading returned null");
    return false;
  }

  if (!modules.PublishService<IThreading>(m_Threading.get()))
  {
    GECKO_ERROR(labels::Platform, "Failed to publish IThreading");
    m_Threading.reset();
    return false;
  }

  return true;
}

void PlatformModule::Shutdown(::gecko::IModuleRegistry& modules) noexcept
{
  GECKO_FUNC(labels::Platform);

  if (m_Threading != nullptr)
  {
    (void)modules.UnpublishService<IThreading>();
    m_Threading.reset();
  }

#if defined(GECKO_PLATFORM_WINDOWS)
  ::timeEndPeriod(1);
#endif
}

}  // namespace gecko::platform
