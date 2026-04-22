#if defined(GECKO_PLATFORM_LINUX)
#define VK_USE_PLATFORM_XLIB_KHR 1

#include "../vulkan_surface.h"
#include "gecko/core/services/log.h"
#include "private/labels.h"

#include <X11/Xlib.h>
#include <vulkan/vulkan.h>

// X11 pollutes the global namespace with macros that collide with our code.
#undef None
#undef Status
#undef Bool
#undef True
#undef False
#undef Expose
#undef DestroyNotify

namespace gecko::graphics {

VkResult CreateXlibSurface(VkInstance                                   instance,
                            const ::gecko::platform::NativeWindowHandle& native,
                            VkSurfaceKHR*                                out) noexcept
{
  VkXlibSurfaceCreateInfoKHR sci{};
  sci.sType  = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
  sci.dpy    = static_cast<Display*>(native.Display);
  sci.window = reinterpret_cast<Window>(native.Handle);

  auto fn = reinterpret_cast<PFN_vkCreateXlibSurfaceKHR>(
      vkGetInstanceProcAddr(instance, "vkCreateXlibSurfaceKHR"));
  if (fn == nullptr)
  {
    GECKO_ERROR(labels::Vulkan,
                "VulkanSurface: vkCreateXlibSurfaceKHR not available");
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }
  return fn(instance, &sci, nullptr, out);
}

}  // namespace gecko::graphics
#endif