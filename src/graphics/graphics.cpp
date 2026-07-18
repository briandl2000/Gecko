#include "gecko/graphics/graphics.h"

#include "gecko/core/ptr.h"
#include "gecko/core/services/log.h"
#include "private/graphics_state.h"
#include "private/labels.h"

namespace gecko::graphics {

namespace {

Unique<GraphicsDevice> g_OwnedDevice;
Unique<IGpuSampler> g_OwnedSampler;
GraphicsDevice* g_Device = nullptr;
IGpuSampler* g_Sampler = nullptr;

}  // namespace

bool detail::Initialize(const GraphicsConfig& config) noexcept
{
  if (g_Device != nullptr)
    return true;

  GraphicsDeviceDesc desc {};
  desc.Backend = config.Backend;
  desc.Debug = config.Debug;
  desc.AppName = config.AppName;
  g_OwnedDevice = CreateGraphicsDevice(desc);
  if (!g_OwnedDevice)
  {
    GECKO_ERROR(labels::Graphics, "Failed to initialize graphics device");
    detail::Shutdown();
    return false;
  }

  g_Device = g_OwnedDevice.get();
  g_OwnedSampler = g_Device->CreateGpuSampler(config.Sampler);
  g_Sampler = g_OwnedSampler.get();
  return true;
}

void detail::Shutdown() noexcept
{
  g_Sampler = nullptr;
  g_OwnedSampler.reset();
  g_Device = nullptr;
  g_OwnedDevice.reset();
}

GraphicsDevice* GetGraphicsDevice() noexcept
{
  return g_Device;
}

IGpuSampler* GetGpuSampler() noexcept
{
  return g_Sampler;
}

}  // namespace gecko::graphics
