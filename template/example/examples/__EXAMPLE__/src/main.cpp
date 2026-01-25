#include <cstdio>

#include "gecko/core/boot.h"
#include "gecko/core/scope.h"
#include "gecko/core/services.h"
#include "gecko/core/services/log.h"
#include "gecko/core/services/modules.h"
#include "gecko/core/utility/thread.h"
#include "gecko/core/version.h"
#include "gecko/runtime/console_log_sink.h"
#include "gecko/runtime/event_bus.h"
#include "gecko/runtime/module_registry.h"
#include "gecko/runtime/ring_logger.h"
#include "gecko/runtime/ring_profiler.h"
#include "gecko/runtime/runtime_module.h"
#include "gecko/runtime/thread_pool_job_system.h"
#include "gecko/runtime/tracking_allocator.h"

namespace app::__EXAMPLE__::labels {
inline constexpr ::gecko::Label App = ::gecko::MakeLabel("app.__EXAMPLE__");
inline constexpr ::gecko::Label Main =
    ::gecko::MakeLabel("app.__EXAMPLE__.main");
}  // namespace app::__EXAMPLE__::labels

namespace {

class ExampleModule final : public ::gecko::IModule
{
public:
  [[nodiscard]] ::gecko::Label RootLabel() const noexcept override
  {
    return app::__EXAMPLE__::labels::App;
  }

  [[nodiscard]] bool Startup(
      ::gecko::IModuleRegistry& modules) noexcept override
  {
    return true;
  }

  void Shutdown(::gecko::IModuleRegistry& modules) noexcept override
  {}
};

ExampleModule g_AppModule;

}  // namespace

static ::gecko::Services CreateServices(
    ::gecko::runtime::TrackingAllocator& trackingAlloc,
    ::gecko::runtime::ThreadPoolJobSystem& jobSystem,
    ::gecko::runtime::RingProfiler& ringProfiler,
    ::gecko::runtime::RingLogger& ringLogger,
    ::gecko::runtime::ModuleRegistry& moduleRegistry,
    ::gecko::runtime::EventBus& eventBus)
{
  ::gecko::Services services {};
  services.Allocator = &trackingAlloc;
  services.JobSystem = &jobSystem;
  services.Profiler = &ringProfiler;
  services.Logger = &ringLogger;
  services.Modules = &moduleRegistry;
  services.EventBus = &eventBus;
  return services;
}

int main()
{
  ::gecko::SystemAllocator systemAlloc;
  ::gecko::runtime::TrackingAllocator trackingAlloc(&systemAlloc);

  ::gecko::runtime::ThreadPoolJobSystem jobSystem;
  jobSystem.SetWorkerThreadCount(4);

  ::gecko::runtime::RingProfiler ringProfiler(1 << 16);
  ::gecko::runtime::RingLogger ringLogger(1024);

  ::gecko::runtime::ModuleRegistry moduleRegistry;
  ::gecko::runtime::EventBus eventBus;

  GECKO_BOOT((CreateServices(trackingAlloc, jobSystem, ringProfiler, ringLogger,
                             moduleRegistry, eventBus)));

  ::gecko::runtime::ConsoleLogSink consoleSink;
  if (auto* logger = ::gecko::GetLogger())
  {
    consoleSink.RegisterWith(logger);
    logger->SetLevel(::gecko::LogLevel::Info);
  }

  GECKO_INFO(app::__EXAMPLE__::labels::Main, "Gecko %s",
             ::gecko::VersionFullString());

  (void)::gecko::InstallModule(::gecko::runtime::GetModule());
  (void)::gecko::InstallModule(g_AppModule);

  GECKO_SLEEP_MS(16);

  consoleSink.Unregister();
  GECKO_SHUTDOWN();
  return 0;
}
