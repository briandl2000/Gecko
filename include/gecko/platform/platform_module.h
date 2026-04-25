#pragma once

#include "gecko/core/services/events.h"
#include "gecko/core/services/jobs.h"
#include "gecko/core/services/log.h"
#include "gecko/core/services/modules.h"
#include "gecko/core/services/profiler.h"

namespace gecko::platform {

namespace labels {
inline constexpr ::gecko::Label Platform = ::gecko::MakeLabel("gecko.platform");
}

// Platform library's module. Stack-construct one and pass &platform into
// Engine::Create({...}). The module owns no service implementations
// itself; it is the lifecycle node for platform-wide setup (timer
// resolution, etc.) and will publish IPlatform-style services in a
// later iteration.
class PlatformModule final : public ::gecko::IModule
{
public:
  [[nodiscard]] constexpr GECKO_API ::gecko::Label RootLabel()
      const noexcept override
  {
    return labels::Platform;
  }

  // Platform startup/shutdown emits diagnostics via GECKO_INFO/WARN/FUNC
  // and may submit jobs / events. Declare the dependency so the
  // registry's topological sort starts the services-publisher first.
  [[nodiscard]] GECKO_API ::std::span<const ::gecko::ServiceId> Requires()
      const noexcept override;

  [[nodiscard]] GECKO_API bool Startup(
      ::gecko::IModuleRegistry& modules) noexcept override;

  GECKO_API void Shutdown(::gecko::IModuleRegistry& modules) noexcept override;
};

}  // namespace gecko::platform
