#include "gecko/runtime/core_module.h"

#include "gecko/core/labels.h"

namespace gecko::runtime {

namespace {

// IDs published by this module. Static storage so we can return a span.
constexpr ServiceId Published[] = {
    ServiceIdOf<IJobSystem>(),
    ServiceIdOf<IProfiler>(),
    ServiceIdOf<ILogger>(),
    ServiceIdOf<IEventBus>(),
};

}  // namespace

CoreModule::CoreModule(IJobSystem& jobs, IProfiler& profiler, ILogger& logger,
                       IEventBus& eventBus) noexcept
    : m_jobs {&jobs}, m_profiler {&profiler}, m_logger {&logger},
      m_eventBus {&eventBus}
{}

Label CoreModule::RootLabel() const noexcept
{
  return MakeLabel("gecko.runtime.core");
}

::std::span<const ServiceId> CoreModule::Publishes() const noexcept
{
  return ::std::span<const ServiceId> {Published};
}

bool CoreModule::Startup(IModuleRegistry& modules) noexcept
{
  if (!m_jobs->Init())
    return false;
  m_jobsInited = true;

  if (!m_profiler->Init())
    return false;
  m_profilerInited = true;

  if (!m_logger->Init())
    return false;
  m_loggerInited = true;

  if (!m_eventBus->Init())
    return false;
  m_eventBusInited = true;

  if (!modules.PublishService<IJobSystem>(m_jobs))
    return false;
  if (!modules.PublishService<IProfiler>(m_profiler))
    return false;
  if (!modules.PublishService<ILogger>(m_logger))
    return false;
  if (!modules.PublishService<IEventBus>(m_eventBus))
    return false;

  return true;
}

void CoreModule::Shutdown(IModuleRegistry& modules) noexcept
{
  modules.UnpublishService<IEventBus>();
  modules.UnpublishService<ILogger>();
  modules.UnpublishService<IProfiler>();
  modules.UnpublishService<IJobSystem>();

  if (m_eventBusInited)
  {
    m_eventBus->Shutdown();
    m_eventBusInited = false;
  }
  if (m_loggerInited)
  {
    m_logger->Shutdown();
    m_loggerInited = false;
  }
  if (m_profilerInited)
  {
    m_profiler->Shutdown();
    m_profilerInited = false;
  }
  if (m_jobsInited)
  {
    m_jobs->Shutdown();
    m_jobsInited = false;
  }
}

}  // namespace gecko::runtime
