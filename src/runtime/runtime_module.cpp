#include "gecko/runtime/runtime_module.h"

#include "gecko/core/labels.h"

namespace gecko::runtime {

namespace {

// IDs published by this module. Static storage so we can return a span.
constexpr ::gecko::ServiceId Published[] = {
    ::gecko::ServiceIdOf<IJobSystem>(),
    ::gecko::ServiceIdOf<IProfiler>(),
    ::gecko::ServiceIdOf<ILogger>(),
    ::gecko::ServiceIdOf<IEventBus>(),
};

}  // namespace

CoreServicesModule::CoreServicesModule(IJobSystem& jobs, IProfiler& profiler,
                                       ILogger& logger,
                                       IEventBus& eventBus) noexcept
    : m_jobs {&jobs}, m_profiler {&profiler}, m_logger {&logger},
      m_eventBus {&eventBus}
{}

::gecko::Label CoreServicesModule::RootLabel() const noexcept
{
  return labels::Runtime;
}

::gecko::Span<const ::gecko::ServiceId> CoreServicesModule::Publishes()
    const noexcept
{
  return ::gecko::Span<const ::gecko::ServiceId> {Published};
}

bool CoreServicesModule::Startup(::gecko::IModuleRegistry& modules) noexcept
{
  // Rollback helper: undoes whatever was already done before a failure
  // mid-Startup. Engine/ModuleRegistry will not call Shutdown() on a
  // module whose Startup() returned false, so we must clean up locally.
  auto rollback = [&]() noexcept {
    // Unpublish in reverse order. UnpublishService is a no-op for ids
    // that were never published, so it's safe to call unconditionally
    // for everything we *might* have reached.
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
  };

  if (!m_jobs->Init())
    return false;
  m_jobsInited = true;

  if (!m_profiler->Init())
  {
    rollback();
    return false;
  }
  m_profilerInited = true;

  if (!m_logger->Init())
  {
    rollback();
    return false;
  }
  m_loggerInited = true;

  if (!m_eventBus->Init())
  {
    rollback();
    return false;
  }
  m_eventBusInited = true;

  if (!modules.PublishService<IJobSystem>(m_jobs))
  {
    rollback();
    return false;
  }
  if (!modules.PublishService<IProfiler>(m_profiler))
  {
    rollback();
    return false;
  }
  if (!modules.PublishService<ILogger>(m_logger))
  {
    rollback();
    return false;
  }
  if (!modules.PublishService<IEventBus>(m_eventBus))
  {
    rollback();
    return false;
  }

  return true;
}

void CoreServicesModule::Shutdown(::gecko::IModuleRegistry& modules) noexcept
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
