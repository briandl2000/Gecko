#pragma once

#include <gecko/core/engine.h>
#include <gecko/core/scope.h>
#include <gecko/core/services/modules.h>
#include <gecko/core/types.h>
#include <gecko/runtime/async_trace_profiler_sink.h>
#include <gecko/runtime/console_log_sink.h>
#include <gecko/runtime/event_bus.h>
#include <gecko/runtime/file_log_sink.h>
#include <gecko/runtime/ring_logger.h>
#include <gecko/runtime/ring_profiler.h>
#include <gecko/runtime/runtime_module.h>
#include <gecko/runtime/thread_pool_job_system.h>
#include <gecko/runtime/tracking_allocator.h>
#include <optional>

namespace gecko::examples::core_example {

/// Top-level application for the Core feature tour.
///
/// Owns every service implementation, every module, and every sink as
/// members so RAII order matches the documented boot/teardown order.
/// Construction boots the engine; `Run()` executes the demos in order;
/// the destructor tears everything down.
class App
{
public:
  App();
  ~App();
  App(const App&) = delete;
  App& operator=(const App&) = delete;

  [[nodiscard]] bool IsValid() const noexcept
  {
    return m_Engine.has_value();
  }

  /// Runs all demo modules and returns the process exit code.
  int Run();

private:
  /// RAII helper: installs the allocator on construction, resets it on
  /// destruction.
  struct AllocatorInstaller
  {
    explicit AllocatorInstaller(::gecko::IAllocator* a) noexcept;
    ~AllocatorInstaller();
    AllocatorInstaller(const AllocatorInstaller&) = delete;
    AllocatorInstaller& operator=(const AllocatorInstaller&) = delete;
    bool Ok = false;
  };

  /// Trivial app module so the engine has somewhere to put the application
  /// label during boot.
  class ExampleModule final : public ::gecko::IModule
  {
  public:
    [[nodiscard]] ::gecko::Label RootLabel() const noexcept override;
    [[nodiscard]] bool Startup(::gecko::IModuleRegistry&) noexcept override;
    void Shutdown(::gecko::IModuleRegistry&) noexcept override;
  };

  void AttachSinks();
  void DetachSinks();

  ::gecko::runtime::TrackingAllocator m_Allocator;
  AllocatorInstaller m_AllocatorInstaller {&m_Allocator};

  ::gecko::runtime::ThreadPoolJobSystem m_JobSystem;
  ::gecko::runtime::RingProfiler m_Profiler {1 << 16};
  ::gecko::runtime::RingLogger m_Logger {1024};
  ::gecko::runtime::EventBus m_EventBus;

  ::gecko::runtime::CoreServicesModule m_RuntimeModule;
  ExampleModule m_AppModule;

  ::std::optional<::gecko::Engine> m_Engine;

  ::gecko::runtime::ConsoleLogSink m_ConsoleSink;
  ::gecko::runtime::FileLogSink m_FileSink {"log.txt"};
  ::gecko::runtime::AsyncTraceProfilerSink m_TraceSink {"gecko_trace.json"};
  bool m_SinksAttached = false;
};

}  // namespace gecko::examples::core_example
