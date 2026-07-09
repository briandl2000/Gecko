#include "gecko/core/engine.h"

#include "gecko/core/assert.h"
#include "gecko/core/scope.h"
#include "gecko/core/services.h"
#include "private/labels.h"
#include "private/module_registry.h"

namespace gecko {

EngineResult Engine::Create(::gecko::Span<IModule*> modules) noexcept
{
  GECKO_PROFILE_NAMED(::gecko::core::labels::Modules, "Engine::Create");
  Engine engine;
  engine.m_registry = ::gecko::Unique<IModuleRegistry>(new (::std::nothrow)::gecko::core::detail::ModuleRegistry());
  if (!engine.m_registry)
  {
    return {};
  }

  if (!engine.m_registry->Init())
  {
    return {};
  }

  // Register every user module. Skip nulls silently -- some app code
  // passes optional modules conditionally.
  for (IModule* m : modules)
  {
    if (m == nullptr)
      continue;
    auto reg = engine.m_registry->RegisterStatic(*m);
    if (!reg.Ok())
    {
      engine.m_registry->Shutdown();
      return {};
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
    return {};
  }

  return EngineResult {::std::move(engine)};
}

EngineResult::EngineResult(Engine engine) noexcept : m_Engine(::std::move(engine)), m_Ok(true)
{}

void EngineResult::reset() noexcept
{
  m_Engine = Engine {};
  m_Ok = false;
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

Engine::Engine(Engine&& other) noexcept : m_registry(::std::move(other.m_registry))
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
  // Precondition: not moved-from. Calling Modules() on a moved-from
  // Engine is a contract violation.
  GECKO_ASSERT(m_registry != nullptr);
  return *m_registry;
}

}  // namespace gecko
