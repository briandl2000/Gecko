#pragma once

#include <gecko/core/engine.h>
#include <gecko/core/scope.h>
#include <gecko/core/services/memory.h>
#include <gecko/core/services/modules.h>
#include <gecko/core/types.h>
#include <gecko/runtime/async_trace_profiler_sink.h>
#include <gecko/runtime/runtime_module.h>
#include <gecko/runtime/standard_log_sinks.h>
#include <gecko/runtime/tracking_allocator.h>
#include <optional>

namespace gecko::examples::core_example {

/// Top-level application for the Core feature tour.
///
/// Owns the allocator, the runtime module (which in turn owns the four
/// foundational services), and one explicit profiler trace sink. RAII
/// member order matches the documented boot/teardown order.
class App
{
public:
  App();
  ~App() = default;

  App(const App&) = delete;
  App& operator=(const App&) = delete;

  [[nodiscard]] bool IsValid() const noexcept
  {
    return m_Engine.has_value();
  }

  /// Runs all demo modules and returns the process exit code.
  int Run();

private:
  /// Trivial app module so the engine has somewhere to put the
  /// application label during boot.
  class ExampleModule final : public ::gecko::IModule
  {
  public:
    [[nodiscard]] ::gecko::Label RootLabel() const noexcept override;
    [[nodiscard]] bool Startup(::gecko::IModuleRegistry&) noexcept override;
    void Shutdown(::gecko::IModuleRegistry&) noexcept override;
  };

  // Order matters: each member depends on what is above it. Reverse
  // destruction gives the correct teardown bracket.

  ::gecko::runtime::TrackingAllocator m_Allocator;
  ::gecko::AllocatorScope m_AllocScope {m_Allocator};

  ::gecko::runtime::RuntimeModule m_RuntimeModule;
  ExampleModule m_AppModule;

  ::std::optional<::gecko::Engine> m_Engine;

  // Standard console + file sinks (auto-attach in ctor, auto-detach in
  // dtor). Engaged after the engine boots in App's ctor body.
  ::std::optional<::gecko::runtime::StandardLogSinks> m_LogSinks;

  // Trace sink is example-specific (Chrome-trace JSON output) so it is
  // wired up explicitly rather than via a helper.
  ::gecko::runtime::AsyncTraceProfilerSink m_TraceSink {"gecko_trace.json"};
};

}  // namespace gecko::examples::core_example
