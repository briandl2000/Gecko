#include "gecko/__MODULE__/__MODULE___module.h"

#include "gecko/core/scope.h"
#include "gecko/core/services/log.h"
#include "private/labels.h"

namespace gecko::__MODULE__ {

constexpr ::gecko::Label __MODULE_CAMEL__Module::RootLabel() const noexcept
{
  return labels::__MODULE_CAMEL__;
}

bool __MODULE_CAMEL__Module::Startup(::gecko::IModuleRegistry& modules) noexcept
{
  GECKO_FUNC(labels::__MODULE_CAMEL__);
  return true;
}

void __MODULE_CAMEL__Module::Shutdown(
    ::gecko::IModuleRegistry& modules) noexcept
{
  GECKO_FUNC(labels::__MODULE_CAMEL__);
}

}  // namespace gecko::__MODULE__
