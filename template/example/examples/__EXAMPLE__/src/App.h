#pragma once

#include <gecko/core/engine.h>
#include <gecko/core/services/memory.h>
#include <gecko/core/services/modules.h>
#include <gecko/core/types.h>
#include <gecko/runtime/runtime_module.h>
#include <gecko/runtime/standard_log_sinks.h>
#include <gecko/runtime/tracking_allocator.h>
#include <optional>

namespace app::__EXAMPLE__ {

/// Minimal Gecko application skeleton.
///
/// Demonstrates the canonical boot pattern:
///
///   1. install the allocator (`AllocatorScope`)
///   2. construct module objects -- each module's default ctor owns
///      sensible production defaults internally; pass a `Backends`
///      struct only when overriding a slot
///   3. `Engine::Create({...})` boots the modules in dependency order
///   4. attach log sinks (`StandardLogSinks`) and run the workload
///   5. shutdown is RAII -- the destructor unregisters sinks, tears
///      down the engine, and resets the allocator in reverse member
///      declaration order
class App
{
public:
  App();
  ~App() = default;

  App(const App&) = delete;
  App& operator=(const App&) = delete;

  /// @return `true` if the engine booted successfully.
  [[nodiscard]] bool IsValid() const noexcept
  {
    return m_Engine.has_value();
  }

  /// Runs the example workload and returns the process exit code.
  int Run();

private:
  /// Trivial app-side module so `Engine::Create` has somewhere to put
  /// the application label.
  class AppModule final : public ::gecko::IModule
  {
  public:
    [[nodiscard]] ::gecko::Label RootLabel() const noexcept override;
    [[nodiscard]] bool Startup(::gecko::IModuleRegistry&) noexcept override;
    void Shutdown(::gecko::IModuleRegistry&) noexcept override;
  };

  // Order matters: each member depends on what is above it. Reverse
  // destruction gives the correct teardown bracket without explicit
  // cleanup code.
  ::gecko::runtime::TrackingAllocator m_Allocator;
  ::gecko::AllocatorScope m_AllocScope {m_Allocator};

  ::gecko::runtime::RuntimeModule m_RuntimeModule;
  AppModule m_AppModule;

  ::std::optional<::gecko::Engine> m_Engine;
  ::std::optional<::gecko::runtime::StandardLogSinks> m_LogSinks;
};

}  // namespace app::__EXAMPLE__
