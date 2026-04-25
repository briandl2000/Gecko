#pragma once

#include "gecko/core/services/modules.h"

namespace gecko::__MODULE__ {

// __MODULE_CAMEL__ library's module. Stack-construct one and pass &it
// into Engine::Create({...}).
class __MODULE_CAMEL__Module final : public ::gecko::IModule
{
public:
  [[nodiscard]] constexpr GECKO_API ::gecko::Label RootLabel()
      const noexcept override;

  [[nodiscard]] GECKO_API bool Startup(
      ::gecko::IModuleRegistry& modules) noexcept override;

  GECKO_API void Shutdown(::gecko::IModuleRegistry& modules) noexcept override;
};

}  // namespace gecko::__MODULE__
