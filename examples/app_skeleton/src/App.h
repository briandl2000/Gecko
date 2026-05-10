#pragma once

#include <gecko/core/engine.h>
#include <gecko/core/scope.h>
#include <gecko/core/services/memory.h>
#include <gecko/core/services/modules.h>
#include <gecko/core/types.h>
#include <gecko/platform/platform_module.h>
#include <gecko/runtime/runtime_module.h>
#include <gecko/runtime/standard_log_sinks.h>
#include <gecko/runtime/tracking_allocator.h>
#include <optional>

namespace gecko::examples::app_skeleton {

/// Runtime configuration parsed from CLI args.
struct AppConfig
{
  const char* title = "Gecko App";
  bool windowed = true;
  ::gecko::u32 maxFrames = 0;  ///< 0 = run until window closes.
  ::gecko::platform::DisplayBackendKind backend = ::gecko::platform::DisplayBackendKind::Auto;
};

/// Minimal Gecko application.
///
/// Demonstrates the canonical boot order every Gecko app follows:
///
///   1. install the allocator (`AllocatorScope`)
///   2. construct module objects (`RuntimeModule`, `PlatformModule`,
///      app module). Each module's default ctor owns sensible
///      production defaults internally; pass a `Backends` struct only
///      when overriding a slot.
///   3. `Engine::Create({...})` boots the modules in dependency order
///   4. attach log sinks (`StandardLogSinks`) and run the main loop
///   5. shutdown is RAII -- the destructor unregisters sinks, tears
///      down the engine, and resets the allocator in reverse member
///      declaration order.
class App
{
public:
  /// Boots the engine using `cfg`. If boot fails, `IsValid()` returns
  /// false and `Run()` will exit with non-zero.
  explicit App(const AppConfig& cfg);
  ~App() = default;

  App(const App&) = delete;
  App& operator=(const App&) = delete;

  /// @return `true` if the engine booted successfully.
  [[nodiscard]] bool IsValid() const noexcept
  {
    return m_Engine.has_value();
  }

  /// Runs the configured workload (windowed or headless) and returns
  /// the process exit code.
  int Run();

private:
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

  AppConfig m_Config;

  // Order matters: each section depends on what is above it. The
  // destructor tears down in reverse member-declaration order, which
  // gives the correct sink-detach -> engine-stop -> allocator-reset
  // bracket without any explicit cleanup code.

  ::gecko::runtime::TrackingAllocator m_Allocator;
  ::gecko::AllocatorScope m_AllocScope {m_Allocator};

  ::gecko::runtime::RuntimeModule m_RuntimeModule;
  ::gecko::platform::PlatformModule m_PlatformModule;
  SkeletonModule m_AppModule;

  // Booted in App's ctor body and torn down implicitly by the dtor.
  // m_LogSinks must declare *after* m_Engine so that it dies first
  // (sinks unregister from the still-alive logger before Engine
  // destroys the registry).
  ::std::optional<::gecko::Engine> m_Engine;
  ::std::optional<::gecko::runtime::StandardLogSinks> m_LogSinks;
};

}  // namespace gecko::examples::app_skeleton
