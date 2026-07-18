#pragma once

#include "gecko/core/api.h"
#include "gecko/graphics/gpu_profiler.h"
#include "gecko/graphics/graphics_device.h"

namespace gecko::graphics {

struct GraphicsConfig
{
  GraphicsBackend Backend {GraphicsBackend::Vulkan};
  bool Debug {false};
  const char* AppName {"Gecko"};
  GpuSamplerDesc Sampler {};
};

[[nodiscard]] GECKO_API GraphicsDevice* GetGraphicsDevice() noexcept;
[[nodiscard]] GECKO_API IGpuSampler* GetGpuSampler() noexcept;

}  // namespace gecko::graphics
