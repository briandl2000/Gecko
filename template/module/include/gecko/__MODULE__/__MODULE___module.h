#pragma once

#include "gecko/core/services/modules.h"

namespace gecko::__MODULE__ {

// __MODULE_CAMEL__ library's module.
class __MODULE_CAMEL__Module final : public ::gecko::IModule
{
public:
  [[nodiscard]] constexpr GECKO_API ::gecko::Label RootLabel()
      const noexcept override;

  [[nodiscard]] GECKO_API bool Startup(
      ::gecko::IModuleRegistry& modules) noexcept override;

  GECKO_API void Shutdown(::gecko::IModuleRegistry& modules) noexcept override;
};

// Explicit registration entry point (called by app/loader after services boot).
[[nodiscard]] GECKO_API ::gecko::ModuleRegistration Install__MODULE_CAMEL__Module(
    ::gecko::IModuleRegistry& modules) noexcept;

// Access the module instance (for unified install flows).
[[nodiscard]] GECKO_API ::gecko::IModule& GetModule() noexcept;

}  // namespace gecko::__MODULE__
