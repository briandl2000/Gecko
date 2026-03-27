#pragma once

#include "gecko/core/api.h"
#include "gecko/core/ptr.h"
#include "gecko/core/services/events.h"
#include "gecko/platform/monitors_interface.h"
#include "gecko/platform/platform_config.h"
#include "gecko/platform/windows_interface.h"

namespace gecko::platform {

class PlatformContext
{
public:
  GECKO_API PlatformContext(const PlatformConfig& cfg);

  GECKO_API ~PlatformContext() = default;

  /// The fully resolved config used to construct this context.
  /// Backend is always a concrete value (never Auto or Unknown).
  GECKO_API const PlatformConfig& Config() const noexcept
  {
    return m_Config;
  }

  GECKO_API IWindowsBackend& Windows()
  {
    return *m_Windows;
  }

  GECKO_API IMonitorsBackend& Monitors()
  {
    return *m_Monitors;
  }

  /// Pump all pending OS events for windows and monitors.
  /// Events are sent to the global event bus — call
  /// DispatchEvents() afterwards to deliver them to Queued subscribers.
  GECKO_API void PumpEvents() noexcept
  {
    m_Windows->PumpEvents(m_Emitter);
    m_Monitors->PumpEvents(m_Emitter);
  }

private:
  PlatformConfig m_Config;
  gecko::EventEmitter m_Emitter {};
  Unique<IWindowsBackend> m_Windows;
  Unique<IMonitorsBackend> m_Monitors;
};

}  // namespace gecko::platform
