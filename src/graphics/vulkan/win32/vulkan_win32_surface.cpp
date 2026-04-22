#if defined(GECKO_PLATFORM_WINDOWS)
#define VK_USE_PLATFORM_WIN32_KHR 1

#ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
#endif

#include "../vulkan_surface.h"
#include "gecko/core/services/log.h"
#include "private/labels.h"

#include <windows.h>
#include <vulkan/vulkan.h>

namespace gecko::graphics {

VkResult CreateWin32Surface(VkInstance                                   instance,
                             const ::gecko::platform::NativeWindowHandle& native,
                             VkSurfaceKHR*                                out) noexcept
{
  VkWin32SurfaceCreateInfoKHR sci{};
  sci.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
  sci.hinstance = ::GetModuleHandle(nullptr);
  sci.hwnd      = static_cast<HWND>(native.Handle);

  auto fn = reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(
      vkGetInstanceProcAddr(instance, "vkCreateWin32SurfaceKHR"));
  if (fn == nullptr)
  {
    GECKO_ERROR(labels::Vulkan,
                "VulkanSurface: vkCreateWin32SurfaceKHR not available");
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }
  return fn(instance, &sci, nullptr, out);
}

}  // namespace gecko::graphics
#endif