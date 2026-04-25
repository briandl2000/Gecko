#pragma once

#include "gecko/core/api.h"
#include "gecko/core/services/events.h"
#include "gecko/core/services/jobs.h"
#include "gecko/core/services/log.h"
#include "gecko/core/services/modules.h"
#include "gecko/core/services/profiler.h"

namespace gecko::runtime {

namespace labels {
inline constexpr ::gecko::Label Runtime = ::gecko::MakeLabel("gecko.runtime");
}

// CoreServicesModule is the lifecycle node for the Runtime library.
//
// The Runtime layer (Core <- Platform <- Runtime <- Program) is where the
// concrete implementations of the four foundational service interfaces
// declared in Core live: IJobSystem, IProfiler, ILogger, IEventBus. It is
// therefore Runtime's job to publish those impls to the registry.
//
// Ownership rule: CoreServicesModule does NOT own the impls — the user
// constructs them on the stack (or wherever) and passes references in.
// CoreServicesModule is responsible for Init/Shutdown and PublishService /
// UnpublishService bookkeeping.
//
// Required by every engine instance: pass &runtimeModule as the first
// module to Engine::Create({...}).
class CoreServicesModule final : public ::gecko::IModule
{
public:
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
