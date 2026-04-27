#include "gecko/graphics/graphics_module.h"

#include "gecko/core/scope.h"
#include "private/labels.h"

namespace gecko::graphics {

namespace {

constexpr ::gecko::ServiceId RequiredServices[] = {
    ::gecko::ServiceIdOf<::gecko::ILogger>(),
    ::gecko::ServiceIdOf<::gecko::IProfiler>(),
    ::gecko::ServiceIdOf<::gecko::IJobSystem>(),
    ::gecko::ServiceIdOf<::gecko::IEventBus>(),
};

}  // namespace

constexpr ::gecko::Label GraphicsModule::RootLabel() const noexcept
{
  return labels::Graphics;
}

::std::span<const ::gecko::ServiceId> GraphicsModule::Requires() const noexcept
{
  return ::std::span<const ::gecko::ServiceId> {RequiredServices};
}

bool GraphicsModule::Startup(::gecko::IModuleRegistry& /*modules*/) noexcept
{
  GECKO_SCOPE(labels::Graphics);
  return true;
}

void GraphicsModule::Shutdown(::gecko::IModuleRegistry& /*modules*/) noexcept
{
  GECKO_SCOPE(labels::Graphics);
}

}  // namespace gecko::graphics
