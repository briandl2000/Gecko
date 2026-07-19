#pragma once

#include "gecko/platform/platform_config.h"

namespace gecko::platform::detail {

[[nodiscard]] bool Initialize(const PlatformConfig& config) noexcept;
void Shutdown() noexcept;

}  // namespace gecko::platform::detail
