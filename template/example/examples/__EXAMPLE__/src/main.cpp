#include "gecko/core/engine.h"
#include "gecko/core/scope.h"
#include "gecko/core/services.h"
#include "gecko/core/services/log.h"
#include "gecko/core/services/modules.h"
#include "gecko/core/utility/thread.h"
#include "gecko/core/version.h"
#include "gecko/runtime/console_log_sink.h"
#include "gecko/runtime/core_module.h"
#include "gecko/runtime/event_bus.h"
#include "gecko/runtime/module_registry.h"
#include "gecko/runtime/ring_logger.h"
#include "gecko/runtime/ring_profiler.h"
#include "gecko/runtime/runtime_module.h"
#include "gecko/runtime/thread_pool_job_system.h"
#include "gecko/runtime/tracking_allocator.h"

#include <cstdio>

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

int main()
{
  ::gecko::runtime::TrackingAllocator trackingAlloc;
  if (!::gecko::SetAllocator(&trackingAlloc))
    return 1;

  ::gecko::runtime::ThreadPoolJobSystem jobSystem;
  jobSystem.SetWorkerThreadCount(4);

  ::gecko::runtime::RingProfiler ringProfiler(1 << 16);
  ::gecko::runtime::RingLogger ringLogger(1024);

  ::gecko::runtime::EventBus eventBus;

  ::gecko::runtime::CoreModule coreModule(jobSystem, ringProfiler, ringLogger,
                                          eventBus);

  auto engine = ::gecko::Engine::Create(
      {&coreModule, &::gecko::runtime::GetModule(), &g_AppModule});
  if (!engine)
  {
    ::gecko::ResetAllocator();
    return 1;
  }

  ::gecko::runtime::ConsoleLogSink consoleSink;
  if (auto* logger = ::gecko::GetLogger())
  {
    consoleSink.RegisterWith(logger);
    logger->SetLevel(::gecko::LogLevel::Info);
  }

  GECKO_INFO(app::__EXAMPLE__::labels::Main, "Gecko %s",
             ::gecko::VersionFullString());

  GECKO_SLEEP_MS(16);

  consoleSink.Unregister();
  engine.reset();
  ::gecko::ResetAllocator();
  return 0;
}
