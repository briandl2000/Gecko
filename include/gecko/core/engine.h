#pragma once

#include "gecko/core/api.h"
#include "gecko/core/services/modules.h"

#include <initializer_list>
#include <memory>
#include <optional>

namespace gecko {

// RAII owner for the engine's module / service lifecycle.
//
// Engine::Create takes the modules to install. Internally it constructs
// a runtime ModuleRegistry, registers each module, then runs Startup in
// dependency order (computed from each module's Requires() / Publishes()
// declarations). On destruction Shutdown runs in reverse order.
//
// Usage:
//
//   ::gecko::SetAllocator(&myAllocator);  // optional, before Create
//
//   ::gecko::runtime::EventBus events;
//   MyJobSystem jobs;
//   MyProfiler  profiler;
//   MyLogger    logger;
//   ::gecko::runtime::CoreServicesModule runtimeModule(jobs, profiler, logger,
//                                                 events);
//   ::gecko::platform::PlatformModule platformModule;
//   MyAppModule  app;
//
//   auto engine = ::gecko::Engine::Create(
//       {&runtimeModule, &platformModule, &app});
//   if (!engine)
//     return 1;
//
//   // ... services are live for the rest of this scope ...
//   // ~Engine() shuts every module down in reverse topological order.
//
// Engine is move-only. A moved-from instance is inert.
class Engine
{
public:
  // Constructs the registry, registers each module, then runs Startup in
  // topological order. Returns std::nullopt if any module fails to start
  // or if the dependency graph is invalid (cycle, missing publisher,
  // duplicate publisher).
  GECKO_API static ::std::optional<Engine> Create(
      ::std::initializer_list<IModule*> modules) noexcept;

  GECKO_API ~Engine() noexcept;

  GECKO_API Engine(Engine&& other) noexcept;
  GECKO_API Engine& operator=(Engine&& other) noexcept;

  Engine(const Engine&) = delete;
  Engine& operator=(const Engine&) = delete;

  // Returns the live module registry. Precondition: this Engine has not
  // been moved from. Calling Modules() on a moved-from instance is a
  // contract violation and triggers an assertion.
  [[nodiscard]] GECKO_API IModuleRegistry& Modules() noexcept;

private:
  Engine() noexcept = default;

  ::std::unique_ptr<IModuleRegistry> m_registry;
};

}  // namespace gecko
