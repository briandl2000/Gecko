#pragma once

#include "gecko/core/services/modules.h"

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

  [[nodiscard]] GECKO_API bool Startup(
      ::gecko::IModuleRegistry& modules) noexcept override;

  GECKO_API void Shutdown(::gecko::IModuleRegistry& modules) noexcept override;
};

}  // namespace gecko::platform
