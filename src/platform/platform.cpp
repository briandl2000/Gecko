#include "gecko/platform/platform.h"

#include "gecko/core/ptr.h"
#include "gecko/core/scope.h"
#include "gecko/core/services/events.h"
#include "gecko/core/services/log.h"
#include "private/labels.h"
#include "private/platform_state.h"
#include "private/window_event_input.h"

#if defined(GECKO_PLATFORM_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <timeapi.h>
#include <Windows.h>
#endif

namespace gecko::platform {

namespace {

Unique<IWindowsBackend> g_OwnedWindows;
Unique<IMonitorsBackend> g_OwnedMonitors;
Unique<IInput> g_OwnedInput;

IWindowsBackend* g_Windows = nullptr;
IMonitorsBackend* g_Monitors = nullptr;
IInput* g_Input = nullptr;
EventEmitter g_Emitter {};
bool g_Initialized = false;

}  // namespace

bool detail::Initialize(const PlatformConfig& requestedConfig) noexcept
{
  if (g_Initialized)
    return true;

#if defined(GECKO_PLATFORM_WINDOWS)
  if (::timeBeginPeriod(1) != TIMERR_NOERROR)
    GECKO_WARN(labels::Platform, "timeBeginPeriod(1) failed");
#endif

  const PlatformConfig config = Resolve(requestedConfig);
  g_OwnedWindows = IWindowsBackend::Create(config);
  g_OwnedMonitors = IMonitorsBackend::Create(config);
  g_OwnedInput = CreateUnique<WindowEventInput>();
  if (!g_OwnedWindows || !g_OwnedMonitors || !g_OwnedInput)
  {
    GECKO_ERROR(labels::Platform, "Failed to initialize platform state");
    detail::Shutdown();
    return false;
  }

  g_Windows = g_OwnedWindows.get();
  g_Monitors = g_OwnedMonitors.get();
  g_Input = g_OwnedInput.get();
  g_Monitors->EnumerateMonitors();

  if (!RegisterEventModule(labels::Platform.Id))
  {
    GECKO_ERROR(labels::Platform, "Failed to register platform event source");
    detail::Shutdown();
    return false;
  }
  g_Emitter = CreateEmitter(labels::Platform.Id);
  g_Initialized = true;
  return true;
}

void detail::Shutdown() noexcept
{
  if (g_Emitter.IsValid())
    UnregisterEventModule(labels::Platform.Id);

  g_Emitter = {};
  g_Input = nullptr;
  g_Monitors = nullptr;
  g_Windows = nullptr;
  g_OwnedInput.reset();
  g_OwnedMonitors.reset();
  g_OwnedWindows.reset();

#if defined(GECKO_PLATFORM_WINDOWS)
  if (g_Initialized)
    ::timeEndPeriod(1);
#endif
  g_Initialized = false;
}

IWindowsBackend* GetWindows() noexcept
{
  return g_Windows;
}

IMonitorsBackend* GetMonitors() noexcept
{
  return g_Monitors;
}

IInput* GetInput() noexcept
{
  return g_Input;
}

void PumpEvents() noexcept
{
  GECKO_PROFILE_NAMED(labels::Platform, "platform::PumpEvents");
  if (g_Windows == nullptr || g_Monitors == nullptr || !g_Emitter.IsValid())
    return;
  if (g_Input != nullptr)
    g_Input->NewFrame();
  g_Windows->PumpEvents(g_Emitter);
  g_Monitors->PumpEvents(g_Emitter);
}

void SetModalFrameCallback(IWindowsBackend::ModalFrameFn callback, void* userData) noexcept
{
  if (g_Windows != nullptr)
    g_Windows->SetModalFrameCallback(callback, userData);
}

}  // namespace gecko::platform
