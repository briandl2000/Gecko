#include "gecko/core/engine.h"

#include "gecko/core/services.h"
#include "private/module_registry.h"

namespace gecko {

::std::optional<Engine> Engine::Create(
    ::std::initializer_list<IModule*> modules) noexcept
{
  Engine engine;
  engine.m_registry = ::std::unique_ptr<IModuleRegistry>(
      new (::std::nothrow)::gecko::core::detail::ModuleRegistry());
  if (!engine.m_registry)
  {
    return ::std::nullopt;
  }

  if (!engine.m_registry->Init())
  {
    return ::std::nullopt;
  }

  // Register every user module. Skip nulls silently — some app code
  // passes optional modules conditionally.
  for (IModule* m : modules)
  {
    if (m == nullptr)
      continue;
    auto reg = engine.m_registry->RegisterStatic(*m);
    if (!reg.Ok())
    {
      engine.m_registry->Shutdown();
      return ::std::nullopt;
    }
    // Engine owns the lifecycle; release the handle so the registry
    // doesn't try to unregister via RAII.
    reg.Handle.Release();
  }

  // Publish the registry globally before Startup so modules can lookup
  // dependencies via GetX() during their Startup() call.
  detail::SetActiveModuleRegistry(engine.m_registry.get());

  if (!engine.m_registry->StartupAllModules())
  {
    detail::SetActiveModuleRegistry(nullptr);
    engine.m_registry->Shutdown();
    return ::std::nullopt;
  }

  return engine;
}

Engine::~Engine() noexcept
{
  if (m_registry)
  {
    m_registry->ShutdownAllModules();
    detail::SetActiveModuleRegistry(nullptr);
    m_registry->Shutdown();
    m_registry.reset();
  }
}

Engine::Engine(Engine&& other) noexcept
    : m_registry(::std::move(other.m_registry))
{
  if (m_registry)
  {
    detail::SetActiveModuleRegistry(m_registry.get());
  }
}

Engine& Engine::operator=(Engine&& other) noexcept
{
  if (this != &other)
  {
    if (m_registry)
    {
      m_registry->ShutdownAllModules();
      detail::SetActiveModuleRegistry(nullptr);
      m_registry->Shutdown();
      m_registry.reset();
    }
    m_registry = ::std::move(other.m_registry);
    if (m_registry)
    {
      detail::SetActiveModuleRegistry(m_registry.get());
    }
  }
  return *this;
}

IModuleRegistry& Engine::Modules() noexcept
{
  return *m_registry;
}

}  // namespace gecko
