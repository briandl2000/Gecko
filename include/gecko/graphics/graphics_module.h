#pragma once

#include "gecko/core/services/events.h"
#include "gecko/core/services/jobs.h"
#include "gecko/core/services/log.h"
#include "gecko/core/services/modules.h"
#include "gecko/core/services/profiler.h"

namespace gecko::graphics {

// Graphics library's module. Stack-construct one and pass it to
// Engine::Create({...}).
class GraphicsModule final : public ::gecko::IModule
{
public:
  [[nodiscard]] constexpr GECKO_API ::gecko::Label RootLabel()
      const noexcept override;

  // Graphics startup/shutdown emits diagnostics. Declare the dependency
  // on the foundational services so the topo sort orders us after the
  // services-publisher (and after PlatformModule, transitively).
  [[nodiscard]] GECKO_API ::std::span<const ::gecko::ServiceId> Requires()
      const noexcept override;

  [[nodiscard]] GECKO_API bool Startup(
      ::gecko::IModuleRegistry& modules) noexcept override;

  GECKO_API void Shutdown(::gecko::IModuleRegistry& modules) noexcept override;
};

}  // namespace gecko::graphics
