#pragma once

#include "gecko/core/api.h"
#include "gecko/core/types.h"
#include "gecko/graphics/graphics.h"
#include "gecko/platform/platform_config.h"

namespace gecko {

enum class InitializeResult : u8
{
  Success,
  AlreadyInitialized,
  OutOfMemory,
  RuntimeFailed,
};

struct GeckoConfig
{
  const char* AppName {"Gecko"};
  platform::PlatformConfig Platform {};
  graphics::GraphicsBackend GraphicsBackend {graphics::GraphicsBackend::Vulkan};
  bool EnableGraphics {true};
  bool EnableGraphicsDebug {false};
};

[[nodiscard]] GECKO_API InitializeResult Initialize(const GeckoConfig& config = {}) noexcept;
GECKO_API void Shutdown() noexcept;
[[nodiscard]] GECKO_API bool IsInitialized() noexcept;

}  // namespace gecko
