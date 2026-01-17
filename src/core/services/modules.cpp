#include "gecko/core/services/modules.h"

#include "gecko/core/services.h"
#include "gecko/core/services/log.h"
#include "gecko/core/services/profiler.h"
#include "private/labels.h"

namespace gecko {

ModuleResult InstallModule(IModule& module) noexcept
{
  GECKO_PROF_FUNC(core::labels::Modules);

  Label rootLabel = module.RootLabel();
  GECKO_INFO(core::labels::Modules, "Installing module '%s'",
             rootLabel.Name ? rootLabel.Name : "<unnamed>");

  ModuleRegistration reg = GetModules()->RegisterStatic(module);
  if (reg.Ok())
  {
    GECKO_DEBUG(core::labels::Modules, "Module '%s' installed successfully",
                rootLabel.Name ? rootLabel.Name : "<unnamed>");
    reg.Handle.Release();
  }
  else
  {
    GECKO_ERROR(core::labels::Modules, "Failed to install module '%s'",
                rootLabel.Name ? rootLabel.Name : "<unnamed>");
  }
  return reg.Result;
}

ModuleHandle::ModuleHandle(IModuleRegistry* modules, Label label) noexcept
    : m_modules {modules}, m_label {label}
{}

ModuleHandle::ModuleHandle(ModuleHandle&& other) noexcept
{
  m_modules = other.m_modules;
  m_label = other.m_label;
  other.m_modules = nullptr;
  other.m_label = {};
}

ModuleHandle& ModuleHandle::operator=(ModuleHandle&& other) noexcept
{
  if (this == &other)
  {
    return *this;
  }

  Reset();

  m_modules = other.m_modules;
  m_label = other.m_label;
  other.m_modules = nullptr;
  other.m_label = {};

  return *this;
}

ModuleHandle::~ModuleHandle() noexcept
{
  Reset();
}

void ModuleHandle::Reset() noexcept
{
  GECKO_PROF_FUNC(core::labels::Modules);

  if (m_modules && m_label.IsValid())
  {
    GECKO_TRACE(core::labels::Modules, "Unregistering module '%s'",
                m_label.Name ? m_label.Name : "<unnamed>");
    (void)m_modules->Unregister(m_label);
  }

  m_modules = nullptr;
  m_label = {};
}

void ModuleHandle::Release() noexcept
{
  m_modules = nullptr;
  m_label = {};
}

bool NullModuleRegistry::Init() noexcept
{
  return true;
}
void NullModuleRegistry::Shutdown() noexcept
{}

ModuleRegistration NullModuleRegistry::RegisterStatic(IModule& module) noexcept
{
  const Label id = module.RootLabel();
  if (!id.IsValid())
  {
    return ModuleRegistration {ModuleHandle {}, ModuleResult::InvalidArgument};
  }
  return ModuleRegistration {MakeHandle(id), ModuleResult::Ok};
}

ModuleResult NullModuleRegistry::Unregister(Label id) noexcept
{
  (void)id;
  return ModuleResult::Ok;
}

IModule* NullModuleRegistry::GetModule(Label id) noexcept
{
  (void)id;
  return nullptr;
}

const IModule* NullModuleRegistry::GetModule(Label id) const noexcept
{
  (void)id;
  return nullptr;
}

void NullModuleRegistry::ForEachModule(ModuleVisitFn fn, void* user) noexcept
{
  (void)fn;
  (void)user;
}

bool NullModuleRegistry::StartupAllModules() noexcept
{
  return true;
}
void NullModuleRegistry::ShutdownAllModules() noexcept
{}

}  // namespace gecko
