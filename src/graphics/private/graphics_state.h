#pragma once

#include "gecko/graphics/graphics.h"

namespace gecko::graphics::detail {

[[nodiscard]] bool Initialize(const GraphicsConfig& config, const char* appName) noexcept;
void Shutdown() noexcept;

}  // namespace gecko::graphics::detail
