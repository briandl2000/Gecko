#pragma once

#include "gecko/core/services/modules.h"

namespace gecko::graphics {

// Graphics library's module. Stack-construct one and pass it to
// Engine::Create({...}).
class GraphicsModule final : public ::gecko::IModule
{
public:
  [[nodiscard]] constexpr GECKO_API ::gecko::Label RootLabel()
      const noexcept override;

  [[nodiscard]] GECKO_API bool Startup(
      ::gecko::IModuleRegistry& modules) noexcept override;

  GECKO_API void Shutdown(::gecko::IModuleRegistry& modules) noexcept override;
};

}  // namespace gecko::graphics
