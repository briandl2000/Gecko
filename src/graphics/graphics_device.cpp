#include "gecko/graphics/graphics_device.h"

#include "private/null_device.h"

#if defined(GECKO_GRAPHICS_VULKAN)
#  include "vulkan/vulkan_device.h"
#endif

namespace gecko::graphics {

Unique<GraphicsDevice> CreateGraphicsDevice() noexcept
{
  return CreateUnique<NullDevice>();
}

Unique<GraphicsDevice> CreateGraphicsDevice(
    const GraphicsDeviceDesc& desc) noexcept
{
  switch (desc.Backend)
  {
#if defined(GECKO_GRAPHICS_VULKAN)
    case GraphicsBackend::Vulkan:
      return CreateUnique<VulkanDevice>(desc);
#endif
    default:
      return CreateUnique<NullDevice>();
  }
}

}  // namespace gecko::graphics
