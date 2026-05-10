#pragma once

/// @file
/// `RuntimeModule` -- the lifecycle node that publishes Core's
/// foundational services (`IJobSystem`, `IProfiler`, `ILogger`,
/// `IEventBus`) at engine startup.

#include "gecko/core/api.h"
#include "gecko/core/ptr.h"
#include "gecko/core/services/events.h"
#include "gecko/core/services/jobs.h"
#include "gecko/core/services/log.h"
#include "gecko/core/services/modules.h"
#include "gecko/core/services/profiler.h"

namespace gecko::runtime {

namespace labels {
/// Module label used by the runtime layer.
inline constexpr ::gecko::Label Runtime = ::gecko::MakeLabel("gecko.runtime");
}  // namespace labels

/// Engine module that owns the foundational service implementations
/// (jobs, profiler, logger, event bus).
///
/// During `Startup()` the module calls `Init` on each service in order
/// (jobs -> profiler -> logger -> events) and publishes them so app
/// code can call `gecko::GetJobSystem()` / `GetProfiler()` /
/// `GetLogger()` / `GetEventBus()`. `Shutdown()` is the reverse.
///
/// Default-construct for sensible defaults (`ThreadPoolJobSystem`,
/// `RingProfiler`, `ImmediateLogger`, `EventBus`); pass a `Backends`
/// struct to inject custom impls. Same pattern as `PlatformModule`
/// and `GraphicsModule`.
///
/// Required by every engine instance: pass `&runtimeModule` as the
/// first module to `Engine::Create({...})`.
class RuntimeModule final : public ::gecko::IModule
{
public:
  /// Optional service injection for tests and specialised hosts.
  ///
  /// The caller owns each non-null impl and must keep it alive for the
  /// lifetime of the `RuntimeModule`. Any field left `nullptr` is
  /// created and owned internally during construction (sensible
  /// production defaults). Production code passes nothing and gets the
  /// defaults.
  struct Backends
  {
    IJobSystem* Jobs = nullptr;     ///< Optional injected job system.
    IProfiler* Profiler = nullptr;  ///< Optional injected profiler.
    ILogger* Logger = nullptr;      ///< Optional injected logger.
    IEventBus* EventBus = nullptr;  ///< Optional injected event bus.
  };

  /// Default-construct with production defaults (owned internally).
  GECKO_API RuntimeModule() noexcept;

  /// Construct with caller-injected backends. Any null field gets a
  /// production default owned by the module.
  GECKO_API explicit RuntimeModule(Backends backends) noexcept;

  /// Explicit-injection ctor (all four impls externally owned).
  GECKO_API RuntimeModule(IJobSystem& jobs, IProfiler& profiler, ILogger& logger, IEventBus& eventBus) noexcept;

  GECKO_API ~RuntimeModule() noexcept override;

  [[nodiscard]] GECKO_API ::gecko::Label RootLabel() const noexcept override;

  [[nodiscard]] GECKO_API bool Startup(::gecko::IModuleRegistry& modules) noexcept override;
  GECKO_API void Shutdown(::gecko::IModuleRegistry& modules) noexcept override;

  [[nodiscard]] GECKO_API ::gecko::Span<const ::gecko::ServiceId> Publishes() const noexcept override;

private:
  // Resolved service pointers (either injected or pointing to the
  // owned defaults below).
  IJobSystem* m_jobs {nullptr};
  IProfiler* m_profiler {nullptr};
  ILogger* m_logger {nullptr};
  IEventBus* m_eventBus {nullptr};

  // Internally-owned defaults; populated by ctors when no impl was
  // injected for the corresponding slot.
  ::gecko::Unique<IJobSystem> m_OwnedJobs;
  ::gecko::Unique<IProfiler> m_OwnedProfiler;
  ::gecko::Unique<ILogger> m_OwnedLogger;
  ::gecko::Unique<IEventBus> m_OwnedEventBus;

  bool m_jobsInited {false};
  bool m_profilerInited {false};
  bool m_loggerInited {false};
  bool m_eventBusInited {false};
};

}  // namespace gecko::runtime
