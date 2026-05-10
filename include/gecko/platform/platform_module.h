#pragma once

/// @file
/// `PlatformModule` and the platform-module service accessors.
///
/// `PlatformModule` is the engine module that owns the windowing and
/// monitor backends. Stack-construct one (optionally with a custom
/// `PlatformConfig`) and pass `&platform` to `Engine::Create({...})`.
/// Filesystem and threading APIs are stateless namespace functions
/// (e.g. `gecko::platform::Read`, `gecko::platform::HardwareThreadCount`)
/// rather than services; see `copilot_context/MODULE_API_SHAPING.md`.

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
/// Module label used by the platform module on the event bus.
inline constexpr ::gecko::Label Platform = ::gecko::MakeLabel("gecko.platform");
}  // namespace labels

/// Engine module that owns the windowing and monitor backends.
///
/// During `Startup()` the module:
/// - Resolves the `PlatformConfig` (`Auto` picks an appropriate backend).
/// - Creates the `IWindowsBackend` + `IMonitorsBackend` (self-installed
///   services -- only one canonical impl per OS, so the module owns them
///   instead of taking them via constructor).
/// - Publishes both as services so application code can call
///   `gecko::platform::GetWindows()` / `GetMonitors()`.
/// - Sets the Win32 multimedia-timer resolution to 1 ms.
class PlatformModule final : public ::gecko::IModule
{
public:
  /// Optional backend injection for tests and specialised hosts.
  ///
  /// The caller owns each non-null backend and must keep it alive for
  /// the lifetime of the `PlatformModule` (same ownership rule as
  /// `RuntimeModule`'s service references). Any backend left
  /// `nullptr` is created and owned internally during `Startup()` from
  /// the resolved `PlatformConfig`. Production code passes nothing and
  /// gets the OS-appropriate defaults.
  ///
  /// The `IInput` service is not exposed here -- there is only one
  /// implementation (`WindowEventInput`) and tests use `MockInput`
  /// directly without going through the module registry.
  struct Backends
  {
    IWindowsBackend* Windows = nullptr;    ///< Optional injected window backend.
    IMonitorsBackend* Monitors = nullptr;  ///< Optional injected monitor backend.
  };

  /// Construct with a `PlatformConfig` and OS-default backends.
  /// @param config  Configuration; defaults to `{}` (auto-select).
  GECKO_API explicit PlatformModule(const PlatformConfig& config = {}) noexcept;
  /// Construct with `config` plus caller-injected `backends`.
  /// @param config    Platform configuration.
  /// @param backends  Backend injection (any null field is auto-created).
  GECKO_API PlatformModule(const PlatformConfig& config, Backends backends) noexcept;
  GECKO_API ~PlatformModule() noexcept override;

  [[nodiscard]] constexpr GECKO_API ::gecko::Label RootLabel() const noexcept override
  {
    return labels::Platform;
  }

  [[nodiscard]] GECKO_API ::gecko::Span<const ::gecko::ServiceId> Requires() const noexcept override;

  [[nodiscard]] GECKO_API ::gecko::Span<const ::gecko::ServiceId> Publishes() const noexcept override;

  [[nodiscard]] GECKO_API bool Startup(::gecko::IModuleRegistry& modules) noexcept override;

  GECKO_API void Shutdown(::gecko::IModuleRegistry& modules) noexcept override;

  /// Resolved config used to construct the backends. `Backend` is
  /// always a concrete value (never `Auto` / `Unknown`) after `Startup`.
  [[nodiscard]] GECKO_API const PlatformConfig& Config() const noexcept
  {
    return m_Config;
  }

private:
  PlatformConfig m_Config;
  ::gecko::EventEmitter m_Emitter {};

  // Externally-owned (injected) backends. Null unless the user passed
  // them via the Backends ctor.
  IWindowsBackend* m_Windows = nullptr;
  IMonitorsBackend* m_Monitors = nullptr;

  // Internally-owned defaults, populated during Startup().
  ::gecko::Unique<IWindowsBackend> m_OwnedWindows;
  ::gecko::Unique<IMonitorsBackend> m_OwnedMonitors;
  ::gecko::Unique<IInput> m_OwnedInput;
};

// Service accessors ------------------------------------------------
//
// Available between `PlatformModule::Startup` and `Shutdown`. Returns
// `nullptr` otherwise.

/// Get the active `IWindowsBackend`, or `nullptr` if not started.
[[nodiscard]] GECKO_API IWindowsBackend* GetWindows() noexcept;
/// Get the active `IMonitorsBackend`, or `nullptr` if not started.
[[nodiscard]] GECKO_API IMonitorsBackend* GetMonitors() noexcept;

// Free functions ---------------------------------------------------

/// Pump pending OS events for windows and monitors. Events are emitted
/// to the global event bus; call `gecko::DispatchEvents()` afterwards
/// to deliver to `Queued` subscribers.
GECKO_API void PumpEvents() noexcept;

/// Register a callback invoked during modal OS loops (e.g. Win32
/// drag/resize) so the application can keep ticking. The callback
/// should perform one frame of work but must NOT call `PumpEvents`.
/// @param callback  Function to invoke each modal-loop tick, or `nullptr` to
/// clear.
/// @param userData  Opaque value passed back to `callback`.
GECKO_API void SetModalFrameCallback(IWindowsBackend::ModalFrameFn callback, void* userData) noexcept;

}  // namespace gecko::platform
