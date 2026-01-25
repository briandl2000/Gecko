#include "gecko/__MODULE__/__MODULE___module.h"

#include "gecko/core/scope.h"
#include "gecko/core/services/log.h"
#include "private/labels.h"

namespace gecko::__MODULE__ {

static __MODULE_CAMEL__Module s___MODULE_CAMEL__Module;

constexpr ::gecko::Label __MODULE_CAMEL__Module::RootLabel() const noexcept
{
  return labels::__MODULE_CAMEL__;
}

bool __MODULE_CAMEL__Module::Startup(::gecko::IModuleRegistry& modules) noexcept
{
  GECKO_FUNC(labels::__MODULE_CAMEL__);
  return true;
}

void __MODULE_CAMEL__Module::Shutdown(::gecko::IModuleRegistry& modules) noexcept
{
  GECKO_FUNC(labels::__MODULE_CAMEL__);
}

::gecko::ModuleRegistration Install__MODULE_CAMEL__Module(
    ::gecko::IModuleRegistry& modules) noexcept
{
  return modules.RegisterStatic(s___MODULE_CAMEL__Module);
}

::gecko::IModule& GetModule() noexcept
{
  return s___MODULE_CAMEL__Module;
}

}  // namespace gecko::__MODULE__
