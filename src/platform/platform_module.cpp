#include "gecko/platform/platform_module.h"

#include "gecko/core/scope.h"
#include "gecko/core/services/log.h"
#include "private/null_input.h"
#include "private/window_event_input.h"

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

constexpr ::gecko::ServiceId kPublishes[] = {
    ::gecko::ServiceIdOf<IWindowsBackend>(),
    ::gecko::ServiceIdOf<IMonitorsBackend>(),
    ::gecko::ServiceIdOf<IInput>(),
};

// File-scope pointers populated by Startup, cleared by Shutdown.
// Free-function accessors (GetWindows/GetMonitors/GetInput/...) read
// these.
IWindowsBackend* g_Windows = nullptr;
IMonitorsBackend* g_Monitors = nullptr;
IInput* g_Input = nullptr;
::gecko::EventEmitter* g_Emitter = nullptr;

}  // namespace

PlatformModule::PlatformModule(const PlatformConfig& config) noexcept
    : m_Config(Resolve(config))
{}

PlatformModule::~PlatformModule() noexcept = default;

::std::span<const ::gecko::ServiceId> PlatformModule::Requires() const noexcept
{
  return ::std::span<const ::gecko::ServiceId> {kRequired};
}

::std::span<const ::gecko::ServiceId> PlatformModule::Publishes() const noexcept
{
  return ::std::span<const ::gecko::ServiceId> {kPublishes};
}

bool PlatformModule::Startup(::gecko::IModuleRegistry& modules) noexcept
{
  GECKO_FUNC(labels::Platform);

#if defined(GECKO_PLATFORM_WINDOWS)
  // Request 1ms timer resolution so SleepNanoseconds is accurate.
  // Without this, Sleep(16) rounds up to ~31ms on default 15.6ms ticks.
  // Restored in Shutdown.
  if (::timeBeginPeriod(1) != TIMERR_NOERROR)
    GECKO_WARN(labels::Platform, "timeBeginPeriod(1) failed");
#endif

  m_Emitter = ::gecko::CreateEmitterForModule(labels::Platform);
  m_Windows = IWindowsBackend::Create(m_Config);
  m_Monitors = IMonitorsBackend::Create(m_Config);
  if (!m_Windows || !m_Monitors)
  {
    GECKO_ERROR(labels::Platform, "Failed to create platform backends");
    return false;
  }
  m_Monitors->EnumerateMonitors();

  // Input service: subscribes to window events on the bus.
  m_Input = ::gecko::CreateUnique<WindowEventInput>();
  if (!m_Input)
  {
    GECKO_ERROR(labels::Platform, "Failed to create input service");
    return false;
  }

  if (!modules.PublishService<IWindowsBackend>(m_Windows.get()))
  {
    GECKO_ERROR(labels::Platform, "PublishService<IWindowsBackend> failed");
    return false;
  }
  if (!modules.PublishService<IMonitorsBackend>(m_Monitors.get()))
  {
    GECKO_ERROR(labels::Platform, "PublishService<IMonitorsBackend> failed");
    (void)modules.UnpublishService<IWindowsBackend>();
    return false;
  }
  if (!modules.PublishService<IInput>(m_Input.get()))
  {
    GECKO_ERROR(labels::Platform, "PublishService<IInput> failed");
    (void)modules.UnpublishService<IMonitorsBackend>();
    (void)modules.UnpublishService<IWindowsBackend>();
    return false;
  }

  g_Windows = m_Windows.get();
  g_Monitors = m_Monitors.get();
  g_Input = m_Input.get();
  g_Emitter = &m_Emitter;

  return true;
}

void PlatformModule::Shutdown(::gecko::IModuleRegistry& modules) noexcept
{
  GECKO_FUNC(labels::Platform);

  g_Emitter = nullptr;
  g_Input = nullptr;
  g_Monitors = nullptr;
  g_Windows = nullptr;

  (void)modules.UnpublishService<IInput>();
  (void)modules.UnpublishService<IMonitorsBackend>();
  (void)modules.UnpublishService<IWindowsBackend>();

  m_Input.reset();
  m_Monitors.reset();
  m_Windows.reset();
  m_Emitter = {};

#if defined(GECKO_PLATFORM_WINDOWS)
  ::timeEndPeriod(1);
#endif
}

// ── Free function accessors ─────────────────────────────────────────

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
  if (!g_Windows || !g_Monitors || !g_Emitter)
    return;
  g_Windows->PumpEvents(*g_Emitter);
  g_Monitors->PumpEvents(*g_Emitter);
}

void SetModalFrameCallback(IWindowsBackend::ModalFrameFn callback,
                           void* userData) noexcept
{
  if (g_Windows)
    g_Windows->SetModalFrameCallback(callback, userData);
}

}  // namespace gecko::platform
