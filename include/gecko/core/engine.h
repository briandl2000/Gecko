#pragma once

/// @file
/// `Engine` -- RAII owner of the module / service lifecycle.

#include "gecko/core/api.h"
#include "gecko/core/services/modules.h"

#include <initializer_list>
#include <memory>
#include <optional>

namespace gecko {

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
/// auto engine = ::gecko::Engine::Create(
///     {&servicesModule, &platformModule, &app});
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
  /// @return The engine on success; `std::nullopt` if any module fails
  ///         to start or the dependency graph is invalid (cycle,
  ///         missing publisher, duplicate publisher).
  ///
  /// abi-ok: `std::optional<Engine>` and `std::initializer_list` cross the
  /// CoreServices DLL boundary. Same-toolchain-per-process is required;
  /// crossing this with a mismatched STL would corrupt. If cross-toolchain
  /// plugins ever need to construct an Engine, switch this to
  /// `Engine* Create(IModule* const* modules, usize count)` and an
  /// out-pointer/`bool` failure path.
  GECKO_API static ::std::optional<Engine> Create(::std::initializer_list<IModule*> modules) noexcept;

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
  Engine() noexcept = default;

  // abi-ok: Engine is moved across the CoreServices DLL boundary by the
  // factory return; this member's layout must match between producer and
  // consumer. Safe under the one-toolchain-per-process rule. A cross-
  // toolchain refactor would PIMPL this behind an opaque `void* m_impl`.
  ::std::unique_ptr<IModuleRegistry> m_registry;
};

}  // namespace gecko
