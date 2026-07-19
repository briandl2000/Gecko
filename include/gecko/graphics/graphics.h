#pragma once

#include "gecko/api.h"
#include "gecko/graphics/command_list.h"
#include "gecko/graphics/gpu_profiler.h"
#include "gecko/graphics/graphics_device.h"
#include "gecko/graphics/graphics_types.h"

namespace gecko::graphics {

struct GraphicsConfig
{
  bool Enabled {true};
  GraphicsBackend Backend {GraphicsBackend::Vulkan};
  bool Debug {false};
  GpuSamplerDesc Sampler {};
};

[[nodiscard]] GECKO_API GraphicsDevice* GetGraphicsDevice() noexcept;
[[nodiscard]] GECKO_API IGpuSampler* GetGpuSampler() noexcept;

}  // namespace gecko::graphics
