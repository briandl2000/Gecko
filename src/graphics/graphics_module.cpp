#include "gecko/graphics/graphics_module.h"

#include "gecko/core/scope.h"
#include "private/labels.h"

namespace gecko::graphics {

constexpr ::gecko::Label GraphicsModule::RootLabel() const noexcept
{
  return labels::Graphics;
}

bool GraphicsModule::Startup(::gecko::IModuleRegistry& /*modules*/) noexcept
{
  GECKO_FUNC(labels::Graphics);
  return true;
}

void GraphicsModule::Shutdown(::gecko::IModuleRegistry& /*modules*/) noexcept
{
  GECKO_FUNC(labels::Graphics);
}

}  // namespace gecko::graphics
