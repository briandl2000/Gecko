#pragma once

#include <gecko/core/engine.h>
#include <gecko/core/services.h>
#include <gecko/core/services/modules.h>
#include <gecko/core/types.h>
#include <gecko/runtime/console_log_sink.h>
#include <gecko/runtime/event_bus.h>
#include <gecko/runtime/ring_logger.h>
#include <gecko/runtime/ring_profiler.h>
#include <gecko/runtime/runtime_module.h>
#include <gecko/runtime/thread_pool_job_system.h>
#include <gecko/runtime/tracking_allocator.h>
#include <optional>

namespace app::debug_renderer_example {

/// Minimal Gecko application skeleton.
///
/// Boot order (each step depends on the one above):
///   1. install allocator
///   2. construct concrete service implementations
///   3. construct module objects (runtime, app)
///   4. `Engine::Create({...})` boots modules in dependency order
///   5. attach log sinks and run the workload
///   6. shutdown is RAII (dtor unregisters sinks, tears down engine, resets
///      the allocator)
class App
{
public:
  App();
  ~App();

  App(const App&) = delete;
  App& operator=(const App&) = delete;

  /// Returns true if the engine booted successfully.
  [[nodiscard]] bool IsValid() const noexcept
  {
    return m_Engine.has_value();
  }

  /// Runs the example workload and returns the process exit code.
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

  /// Trivial app-side module so `Engine::Create` has somewhere to put
  /// the application label.
  class AppModule final : public ::gecko::IModule
  {
  public:
    [[nodiscard]] ::gecko::Label RootLabel() const noexcept override;
    [[nodiscard]] bool Startup(::gecko::IModuleRegistry&) noexcept override;
    void Shutdown(::gecko::IModuleRegistry&) noexcept override;
  };

  void AttachSinks();
  void DetachSinks();

  // Order matters: each section depends on what is above it.
  ::gecko::runtime::TrackingAllocator m_Allocator;
  AllocatorInstaller m_AllocatorInstaller {&m_Allocator};

  ::gecko::runtime::ThreadPoolJobSystem m_JobSystem;
  ::gecko::runtime::RingProfiler m_Profiler {1 << 16};
  ::gecko::runtime::RingLogger m_Logger {1024};
  ::gecko::runtime::EventBus m_EventBus;

  ::gecko::runtime::CoreServicesModule m_RuntimeModule;
  AppModule m_AppModule;

  ::std::optional<::gecko::Engine> m_Engine;

  ::gecko::runtime::ConsoleLogSink m_ConsoleSink;
  bool m_SinksAttached = false;
};

}  // namespace app::debug_renderer_example
