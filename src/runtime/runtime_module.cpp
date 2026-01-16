#include "gecko/runtime/runtime_module.h"

#include "labels.h"

namespace gecko::runtime {

static RuntimeModule s_RuntimeModule;

constexpr ::gecko::Label RuntimeModule::RootLabel() const noexcept
{
  return labels::Runtime;
}

bool RuntimeModule::Startup(::gecko::IModuleRegistry& modules) noexcept
{
  return true;
}

void RuntimeModule::Shutdown(::gecko::IModuleRegistry& modules) noexcept
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
