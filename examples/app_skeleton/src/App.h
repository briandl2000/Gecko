#pragma once

#include <gecko/core/engine.h>
#include <gecko/core/scope.h>
#include <gecko/core/services/modules.h>
#include <gecko/core/types.h>
#include <gecko/platform/platform_module.h>
#include <gecko/runtime/console_log_sink.h>
#include <gecko/runtime/event_bus.h>
#include <gecko/runtime/file_log_sink.h>
#include <gecko/runtime/ring_logger.h>
#include <gecko/runtime/ring_profiler.h>
#include <gecko/runtime/runtime_module.h>
#include <gecko/runtime/thread_pool_job_system.h>
#include <gecko/runtime/tracking_allocator.h>
#include <optional>

namespace gecko::examples::app_skeleton {

/// Runtime configuration parsed from CLI args.
struct AppConfig
{
  const char* title = "Gecko App";
  bool windowed = true;
  ::gecko::u32 maxFrames = 0;  ///< 0 = run until window closes.
  ::gecko::platform::DisplayBackendKind backend =
      ::gecko::platform::DisplayBackendKind::Auto;
};

/// Minimal Gecko application.
///
/// Demonstrates the boot order every Gecko app follows:
///
///   1. install allocator (infrastructure)
///   2. construct concrete service implementations
///   3. construct module objects (runtime, platform, app)
///   4. `Engine::Create({...})` boots modules in dependency order
///   5. attach log sinks and run the main loop
///   6. shutdown is RAII (dtor unregisters sinks, tears down engine, resets
///      the allocator)
///
/// All services and modules are owned as members of this class. The
/// declaration order of those members is intentional — see App.cpp.
class App
{
public:
  /// Boots the engine using `cfg`. Throws nothing; if boot fails,
  /// `IsValid()` returns false and `Run()` will exit with non-zero.
  explicit App(const AppConfig& cfg);
  ~App();

  App(const App&) = delete;
  App& operator=(const App&) = delete;

  /// Returns true if the engine booted successfully.
  [[nodiscard]] bool IsValid() const noexcept
  {
    return m_Engine.has_value();
  }

  /// Runs the configured workload (windowed or headless) and returns the
  /// process exit code.
  int Run();

private:
  /// RAII helper: installs the allocator on construction, resets it on
  /// destruction. Declared between the allocator member and the rest so
  /// member construction order produces the right install/reset bracket.
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
  class SkeletonModule final : public ::gecko::IModule
  {
  public:
    [[nodiscard]] ::gecko::Label RootLabel() const noexcept override;
    [[nodiscard]] bool Startup(::gecko::IModuleRegistry&) noexcept override;
    void Shutdown(::gecko::IModuleRegistry&) noexcept override;
  };

  void RunHeadless();
  void RunWindowed();
  void AttachSinks();
  void DetachSinks();

  AppConfig m_Config;

  // Order matters: each section depends on what is above it.
  ::gecko::runtime::TrackingAllocator m_Allocator;
  AllocatorInstaller m_AllocatorInstaller {&m_Allocator};

  ::gecko::runtime::ThreadPoolJobSystem m_JobSystem;
  ::gecko::runtime::RingProfiler m_Profiler {1 << 16};
  ::gecko::runtime::RingLogger m_Logger {1024};
  ::gecko::runtime::EventBus m_EventBus;

  ::gecko::runtime::CoreServicesModule m_RuntimeModule;
  ::gecko::platform::PlatformModule m_PlatformModule;
  SkeletonModule m_AppModule;

  ::std::optional<::gecko::Engine> m_Engine;

  ::gecko::runtime::ConsoleLogSink m_ConsoleSink;
  ::gecko::runtime::FileLogSink m_FileSink {"log.txt"};
  bool m_SinksAttached = false;
};

}  // namespace gecko::examples::app_skeleton
