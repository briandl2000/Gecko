#pragma once

#include "gecko/core/services/modules.h"

namespace gecko::debug_renderer {

namespace labels {
/// Module label used by the debug renderer module.
inline constexpr ::gecko::Label DebugRenderer =
    ::gecko::MakeLabel("gecko.debug_renderer");
}  // namespace labels

// DebugRenderer library's module. Stack-construct one and pass &it
// into Engine::Create({...}).
class DebugRendererModule final : public ::gecko::IModule
{
public:
  [[nodiscard]] ::gecko::Span<const ::gecko::ServiceId> Requires()
      const noexcept override;

  [[nodiscard]] constexpr GECKO_API ::gecko::Label RootLabel() const noexcept
  {
    return labels::DebugRenderer;
  }

  [[nodiscard]] GECKO_API bool Startup(
      ::gecko::IModuleRegistry& modules) noexcept override;

  GECKO_API void Shutdown(::gecko::IModuleRegistry& modules) noexcept override;
};

}  // namespace gecko::debug_renderer
