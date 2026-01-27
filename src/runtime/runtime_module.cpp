#include "gecko/runtime/runtime_module.h"

namespace gecko::runtime {

static RuntimeModule s_RuntimeModule;

bool RuntimeModule::Startup(::gecko::IModuleRegistry& /*modules*/) noexcept
{
  return true;
}

void RuntimeModule::Shutdown(::gecko::IModuleRegistry& /*modules*/) noexcept
{}

::gecko::ModuleRegistration InstallRuntimeModule(
    ::gecko::IModuleRegistry& modules) noexcept
{
  return modules.RegisterStatic(s_RuntimeModule);
}

::gecko::IModule& GetModule() noexcept
{
  return s_RuntimeModule;
}

}  // namespace gecko::runtime
