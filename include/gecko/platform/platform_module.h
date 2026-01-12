#pragma once

#include "gecko/core/modules.h"

namespace gecko::platform {

// Platform library's module.
class PlatformModule final : public ::gecko::IModule {
public:
  [[nodiscard]] constexpr GECKO_API ::gecko::Label
  RootLabel() const noexcept override;

  [[nodiscard]] GECKO_API bool
  Startup(::gecko::IModuleRegistry &modules) noexcept override;

  GECKO_API void Shutdown(::gecko::IModuleRegistry &modules) noexcept override;
};

// Explicit registration entry point (called by app/loader after services boot).
[[nodiscard]] GECKO_API ::gecko::ModuleRegistration
InstallPlatformModule(::gecko::IModuleRegistry &modules) noexcept;

// Access the platform module instance (for unified install flows).
[[nodiscard]] GECKO_API ::gecko::IModule &GetModule() noexcept;

} // namespace gecko::platform
