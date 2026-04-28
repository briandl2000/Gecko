#pragma once

/// @file
/// `CoreServicesModule` — the lifecycle node that publishes Core's
/// foundational services (`IJobSystem`, `IProfiler`, `ILogger`,
/// `IEventBus`) at engine startup.

#include "gecko/core/api.h"
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

/// Lifecycle node for the Runtime library.
///
/// The Runtime layer (Core <- Platform <- Runtime <- Program) is
/// where the concrete implementations of Core's four foundational
/// service interfaces live: `IJobSystem`, `IProfiler`, `ILogger`,
/// `IEventBus`. `CoreServicesModule` calls `Init`/`Shutdown` on each
/// and publishes them to the module registry.
///
/// **Ownership:** `CoreServicesModule` does NOT own the impls. The
/// caller constructs them on the stack (or wherever) and passes
/// references in. Each impl must outlive the module.
///
/// Required by every engine instance: pass `&runtimeModule` as the
/// first module to `Engine::Create({...})`.
class CoreServicesModule final : public ::gecko::IModule
{
public:
  /// Wire references to the four foundational services.
  /// @param jobs      Job system implementation.
  /// @param profiler  Profiler implementation.
  /// @param logger    Logger implementation.
  /// @param eventBus  Event bus implementation.
  GECKO_API CoreServicesModule(IJobSystem& jobs, IProfiler& profiler,
                               ILogger& logger, IEventBus& eventBus) noexcept;

  [[nodiscard]] GECKO_API ::gecko::Label RootLabel() const noexcept override;

  [[nodiscard]] GECKO_API bool Startup(
      ::gecko::IModuleRegistry& modules) noexcept override;
  GECKO_API void Shutdown(::gecko::IModuleRegistry& modules) noexcept override;

  [[nodiscard]] GECKO_API ::std::span<const ::gecko::ServiceId> Publishes()
      const noexcept override;

private:
  IJobSystem* m_jobs {nullptr};
  IProfiler* m_profiler {nullptr};
  ILogger* m_logger {nullptr};
  IEventBus* m_eventBus {nullptr};

  bool m_jobsInited {false};
  bool m_profilerInited {false};
  bool m_loggerInited {false};
  bool m_eventBusInited {false};
};

}  // namespace gecko::runtime
