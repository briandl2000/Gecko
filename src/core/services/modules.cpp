#include "gecko/core/services/modules.h"

#include "gecko/core/scope.h"
#include "gecko/core/services.h"
#include "gecko/core/services/log.h"
#include "private/labels.h"

namespace gecko {

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
    return *this;

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
  m_modules = nullptr;
  m_label = {};
}

void ModuleHandle::Release() noexcept
{
  m_modules = nullptr;
  m_label = {};
}

// NullModuleRegistry - no profiling, simple no-ops
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
    return ModuleRegistration {{}, ModuleResult::InvalidArgument};
  return ModuleRegistration {MakeHandle(id), ModuleResult::Ok};
}

ModuleResult NullModuleRegistry::Unregister(Label) noexcept
{
  return ModuleResult::Ok;
}

IModule* NullModuleRegistry::GetModule(Label) noexcept
{
  return nullptr;
}
const IModule* NullModuleRegistry::GetModule(Label) const noexcept
{
  return nullptr;
}
void NullModuleRegistry::ForEachModule(ModuleVisitFn, void*) noexcept
{}
bool NullModuleRegistry::StartupAllModules() noexcept
{
  return true;
}
void NullModuleRegistry::ShutdownAllModules() noexcept
{}

bool NullModuleRegistry::PublishServiceImpl(ServiceId, void*) noexcept
{
  return false;
}
void* NullModuleRegistry::GetServiceImpl(ServiceId) const noexcept
{
  return nullptr;
}
bool NullModuleRegistry::UnpublishServiceImpl(ServiceId) noexcept
{
  return false;
}

}  // namespace gecko
