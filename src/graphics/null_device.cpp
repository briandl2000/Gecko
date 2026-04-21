#include "gecko/graphics/device.h"

#include "private/null_device.h"

namespace gecko::graphics {

static NullDevice s_NullDevice;
static IDevice*   s_ActiveDevice = &s_NullDevice;

IDevice& GetDevice() noexcept
{
  return *s_ActiveDevice;
}

void InstallDevice(IDevice* device) noexcept
{
  s_ActiveDevice = (device != nullptr) ? device : &s_NullDevice;
}

}  // namespace gecko::graphics
