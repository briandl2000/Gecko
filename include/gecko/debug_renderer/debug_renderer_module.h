#pragma once

/// @file
/// `DebugRendererModule` -- engine module that owns the shared
/// graphics pipeline used by every `DebugRendererContext` in the
/// process. Stack-construct one and pass it to `Engine::Create({...})`.

#include "gecko/core/api.h"
#include "gecko/core/labels.h"
#include "gecko/core/services/modules.h"

namespace gecko::debug_renderer {

namespace labels {
/// Module label used by the debug renderer.
inline constexpr ::gecko::Label DebugRenderer =
    ::gecko::MakeLabel("gecko.debug_renderer");
}  // namespace labels

/// Engine module for the debug renderer library.
///
/// During `Startup()` the module creates the shared GPU pipeline used
/// to rasterise debug lines. The module publishes no services -- it
/// exists purely as a lifecycle node and a container for the shared
/// pipeline. Per-frame line state lives on `DebugRendererContext`,
/// which the application owns.
///
/// Requires: `GraphicsDevice` (i.e. `GraphicsModule` must be ahead of
/// it in the module list).
class DebugRendererModule final : public ::gecko::IModule
{
public:
  [[nodiscard]] GECKO_API ::gecko::Span<const ::gecko::ServiceId> Requires()
      const noexcept override;

  [[nodiscard]] constexpr GECKO_API ::gecko::Label RootLabel()
      const noexcept override
  {
    return labels::DebugRenderer;
  }

  [[nodiscard]] GECKO_API bool Startup(
      ::gecko::IModuleRegistry& modules) noexcept override;

  GECKO_API void Shutdown(::gecko::IModuleRegistry& modules) noexcept override;
};

}  // namespace gecko::debug_renderer
