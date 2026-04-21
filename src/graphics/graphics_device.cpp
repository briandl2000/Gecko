#include "gecko/graphics/graphics_device.h"

#include "private/null_device.h"

namespace gecko::graphics {

Unique<GraphicsDevice> CreateGraphicsDevice() noexcept
{
  return CreateUnique<NullDevice>();
}

}  // namespace gecko::graphics
