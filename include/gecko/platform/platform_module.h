#pragma once

#include "gecko/core/ptr.h"
#include "gecko/core/services/events.h"
#include "gecko/core/services/jobs.h"
#include "gecko/core/services/log.h"
#include "gecko/core/services/modules.h"
#include "gecko/core/services/profiler.h"
#include "gecko/platform/input.h"
#include "gecko/platform/monitors_interface.h"
#include "gecko/platform/platform_config.h"
#include "gecko/platform/windows_interface.h"

namespace gecko::platform {

namespace labels {
inline constexpr ::gecko::Label Platform = ::gecko::MakeLabel("gecko.platform");
}

// Platform library's module. Stack-construct one (optionally passing the
// desired PlatformConfig) and pass &platform into Engine::Create({...}).
//
// During Startup() the module:
//   - Resolves the PlatformConfig (Auto picks an appropriate backend).
//   - Creates the IWindowsBackend + IMonitorsBackend (self-installed
//     services — only one canonical impl per OS, so the module owns them
//     instead of taking them via constructor).
//   - Publishes both as services so the rest of the app can call
//     gecko::platform::GetWindows() / GetMonitors().
//   - Sets Win32 multimedia timer resolution to 1ms.
//
// Filesystem and threading APIs are stateless namespace functions
// (gecko::platform::Read, gecko::platform::HardwareThreadCount, ...)
// not services — see docs/CODING_STANDARDS.md ("Module API Shaping").
class PlatformModule final : public ::gecko::IModule
{
public:
  GECKO_API explicit PlatformModule(const PlatformConfig& config = {}) noexcept;
  GECKO_API ~PlatformModule() noexcept override;

  [[nodiscard]] constexpr GECKO_API ::gecko::Label RootLabel()
      const noexcept override
  {
    return labels::Platform;
  }

  [[nodiscard]] GECKO_API ::std::span<const ::gecko::ServiceId> Requires()
      const noexcept override;

  [[nodiscard]] GECKO_API ::std::span<const ::gecko::ServiceId> Publishes()
      const noexcept override;

  [[nodiscard]] GECKO_API bool Startup(
      ::gecko::IModuleRegistry& modules) noexcept override;

  GECKO_API void Shutdown(::gecko::IModuleRegistry& modules) noexcept override;

  // Resolved config used to construct the backends. Backend is always a
  // concrete value (never Auto / Unknown) after Startup.
  [[nodiscard]] GECKO_API const PlatformConfig& Config() const noexcept
  {
    return m_Config;
  }

private:
  PlatformConfig m_Config;
  ::gecko::EventEmitter m_Emitter {};
  ::gecko::Unique<IWindowsBackend> m_Windows;
  ::gecko::Unique<IMonitorsBackend> m_Monitors;
  ::gecko::Unique<IInput> m_Input;
};

// ── Service accessors ────────────────────────────────────────────────
//
// Available between PlatformModule::Startup and Shutdown. Returns
// nullptr otherwise.

[[nodiscard]] GECKO_API IWindowsBackend* GetWindows() noexcept;
[[nodiscard]] GECKO_API IMonitorsBackend* GetMonitors() noexcept;

// ── Free functions ───────────────────────────────────────────────────

// Pump pending OS events for windows + monitors. Events are emitted to
// the global event bus; call gecko::DispatchEvents() afterwards to
// deliver to Queued subscribers.
GECKO_API void PumpEvents() noexcept;

// Register a callback invoked during modal OS loops (e.g. Win32
// drag/resize) so the application can keep ticking. The callback should
// perform one frame of work but NOT call PumpEvents.
GECKO_API void SetModalFrameCallback(IWindowsBackend::ModalFrameFn callback,
                                     void* userData) noexcept;

}  // namespace gecko::platform
