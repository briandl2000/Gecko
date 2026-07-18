#if defined(GECKO_PLATFORM_LINUX)
#define VK_USE_PLATFORM_WAYLAND_KHR 1

#include "../../private/labels.h"
#include "gecko/core/services/log.h"
#include "gecko/platform/window.h"

#include <vulkan/vulkan.h>
#include <wayland-client.h>

namespace gecko::graphics {

VkResult CreateWaylandSurface(VkInstance instance, const ::gecko::platform::NativeWindowHandle& native,
                              VkSurfaceKHR* out) noexcept
{
  VkWaylandSurfaceCreateInfoKHR sci {};
  sci.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
  sci.display = static_cast<struct wl_display*>(native.Display);
  sci.surface = static_cast<struct wl_surface*>(native.Handle);

  auto fn =
      reinterpret_cast<PFN_vkCreateWaylandSurfaceKHR>(vkGetInstanceProcAddr(instance, "vkCreateWaylandSurfaceKHR"));
  if (fn == nullptr)
  {
    GECKO_ERROR(labels::Vulkan, "VulkanSurface: vkCreateWaylandSurfaceKHR not available");
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }
  return fn(instance, &sci, nullptr, out);
}

}  // namespace gecko::graphics
#endif
