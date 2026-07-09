#pragma once

/// @file
/// `Engine` -- RAII owner of the module / service lifecycle.

#include "gecko/core/api.h"
#include "gecko/core/ptr.h"
#include "gecko/core/services/modules.h"
#include "gecko/core/span.h"

namespace gecko {

class EngineResult;

/// RAII owner for the engine's module / service lifecycle.
///
/// `Engine::Create` takes the list of modules to install. Internally
/// it constructs a runtime `ModuleRegistry`, registers each module,
/// then runs `Startup` in dependency order (computed from each
/// module's `Requires()` / `Publishes()` declarations). On destruction
/// `Shutdown` runs in reverse order.
///
/// `Engine` is move-only; a moved-from instance is inert.
///
/// @par Usage
/// @code
/// ::gecko::SetAllocator(&myAllocator);  // optional, before Create
///
/// ::gecko::runtime::EventBus events;
/// MyJobSystem jobs;
/// MyProfiler  profiler;
/// MyLogger    logger;
/// ::gecko::runtime::RuntimeModule servicesModule(jobs, profiler,
/// logger,
///                                                    events);
/// ::gecko::platform::PlatformModule platformModule;
/// MyAppModule  app;
///
/// ::gecko::IModule* modules[] = {&servicesModule, &platformModule, &app};
/// auto engine = ::gecko::Engine::Create(modules);
/// if (!engine)
///   return 1;
///
/// // ... services are live for the rest of this scope ...
/// // ~Engine() shuts every module down in reverse topological order.
/// @endcode
class Engine
{
public:
  /// Construct an engine and start every module.
  ///
  /// Constructs the registry, registers each supplied module, then
  /// runs `Startup` in topological order.
  ///
  /// @param modules Modules to install. Order is irrelevant; the
  ///        registry topologically sorts them.
  /// @return The engine on success; empty result if any module fails
  ///         to start or the dependency graph is invalid (cycle,
  ///         missing publisher, duplicate publisher).
  GECKO_API static EngineResult Create(::gecko::Span<IModule*> modules) noexcept;

  /// Runs `Shutdown` on every module in reverse topological order.
  GECKO_API ~Engine() noexcept;

  GECKO_API Engine(Engine&& other) noexcept;
  GECKO_API Engine& operator=(Engine&& other) noexcept;

  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;

  /// @return The live module registry.
  /// @pre This `Engine` has not been moved from. Calling on a
  ///      moved-from instance fires a `GECKO_ASSERT`.
  [[nodiscard]] GECKO_API IModuleRegistry& Modules() noexcept;

private:
  friend class EngineResult;

  Engine() noexcept = default;

  ::gecko::Unique<IModuleRegistry> m_registry;
};

class EngineResult
{
public:
  EngineResult() noexcept = default;
  GECKO_API explicit EngineResult(Engine engine) noexcept;

  EngineResult(const EngineResult&) = delete;
  EngineResult& operator=(const EngineResult&) = delete;
  EngineResult(EngineResult&&) noexcept = default;
  EngineResult& operator=(EngineResult&&) noexcept = default;
  ~EngineResult() noexcept = default;

  [[nodiscard]] bool has_value() const noexcept
  {
    return m_Ok;
  }
  explicit operator bool() const noexcept
  {
    return m_Ok;
  }
  [[nodiscard]] Engine* operator->() noexcept
  {
    return &m_Engine;
  }
  [[nodiscard]] const Engine* operator->() const noexcept
  {
    return &m_Engine;
  }
  [[nodiscard]] Engine& operator*() noexcept
  {
    return m_Engine;
  }
  [[nodiscard]] const Engine& operator*() const noexcept
  {
    return m_Engine;
  }
  GECKO_API void reset() noexcept;

private:
  Engine m_Engine {};
  bool m_Ok {false};
};

}  // namespace gecko
