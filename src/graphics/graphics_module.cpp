#include "gecko/graphics/graphics_module.h"

#include "gecko/core/scope.h"
#include "gecko/core/services/log.h"
#include "private/labels.h"

namespace gecko::graphics {

static GraphicsModule s_GraphicsModule;

constexpr ::gecko::Label GraphicsModule::RootLabel() const noexcept
{
  return labels::Graphics;
}

bool GraphicsModule::Startup(::gecko::IModuleRegistry& /*modules*/) noexcept
{
  GECKO_FUNC(labels::Graphics);
  return true;
}

void GraphicsModule::Shutdown(
    ::gecko::IModuleRegistry& /*modules*/) noexcept
{
  GECKO_FUNC(labels::Graphics);
}

::gecko::ModuleRegistration InstallGraphicsModule(
    ::gecko::IModuleRegistry& modules) noexcept
{
  return modules.RegisterStatic(s_GraphicsModule);
}

::gecko::IModule& GetModule() noexcept
{
  return s_GraphicsModule;
}

}  // namespace gecko::graphics
