#include "gecko/platform/platform_module.h"

#include "gecko/core/scope.h"
#include "gecko/core/services/log.h"
#include "private/labels.h"

namespace gecko::platform {

static PlatformModule s_PlatformModule;

constexpr ::gecko::Label PlatformModule::RootLabel() const noexcept
{
  return labels::Platform;
}

bool PlatformModule::Startup(::gecko::IModuleRegistry& /*modules*/) noexcept
{
  GECKO_FUNC(labels::Platform);
  // Minimal placeholder extension point.
  return true;
}

void PlatformModule::Shutdown(::gecko::IModuleRegistry& /*modules*/) noexcept
{
  GECKO_FUNC(labels::Platform);
}

::gecko::ModuleRegistration InstallPlatformModule(
    ::gecko::IModuleRegistry& modules) noexcept
{
  return modules.RegisterStatic(s_PlatformModule);
}

::gecko::IModule& GetModule() noexcept
{
  return s_PlatformModule;
}

}  // namespace gecko::platform
