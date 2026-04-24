#pragma once

#include "gecko/core/api.h"
#include "gecko/core/services/events.h"
#include "gecko/core/services/jobs.h"
#include "gecko/core/services/log.h"
#include "gecko/core/services/modules.h"
#include "gecko/core/services/profiler.h"

namespace gecko::runtime {

// CoreModule publishes the four foundational services (JobSystem,
// Profiler, Logger, EventBus) that the rest of the engine depends on.
// It owns no implementations — the caller passes references to the
// concrete impls and CoreModule is responsible for Init/Shutdown plus
// publishing them via the module registry.
//
// Required by every engine instance: pass &core as the first module to
// Engine::Create({...}).
class CoreModule final : public IModule
{
public:
  GECKO_API CoreModule(IJobSystem& jobs, IProfiler& profiler, ILogger& logger,
                       IEventBus& eventBus) noexcept;

  [[nodiscard]] GECKO_API Label RootLabel() const noexcept override;

  [[nodiscard]] GECKO_API bool Startup(
      IModuleRegistry& modules) noexcept override;
  GECKO_API void Shutdown(IModuleRegistry& modules) noexcept override;

  [[nodiscard]] GECKO_API ::std::span<const ServiceId> Publishes()
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
