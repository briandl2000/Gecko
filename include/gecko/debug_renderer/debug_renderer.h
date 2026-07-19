#pragma once

#include "gecko/api.h"
#include "gecko/core/types.h"
#include "gecko/graphics/command_list.h"
#include "gecko/graphics/graphics_types.h"
#include "gecko/math/vector.h"

namespace gecko::debug_renderer {

inline constexpr u32 DefaultLineCapacity = 16'384;

[[nodiscard]] GECKO_API bool Initialize(
    graphics::DataFormat renderTargetFormat = graphics::DataFormat::R8G8B8A8_UNORM,
    u32 lineCapacity = DefaultLineCapacity) noexcept;
GECKO_API void Shutdown() noexcept;
[[nodiscard]] GECKO_API bool IsInitialized() noexcept;

GECKO_API void BeginFrame() noexcept;
GECKO_API void DrawLine(math::Float2 a, math::Float2 b, math::Float3 color, f32 thickness = 1.0F) noexcept;
GECKO_API void Submit(graphics::ICommandList& commandList, const graphics::RenderTarget& target) noexcept;

}  // namespace gecko::debug_renderer
