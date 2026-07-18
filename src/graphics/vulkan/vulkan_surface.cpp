#if defined(GECKO_GRAPHICS_VULKAN)
#include "vulkan_surface.h"

#include "../private/labels.h"
#include "gecko/core/services/log.h"
#include "gecko/platform/platform_config.h"

namespace gecko::graphics {

// -- Per-OS backends (defined in vulkan/<os>/vulkan_*_surface.cpp) ---------

#if defined(GECKO_GRAPHICS_VULKAN_XLIB)
VkResult CreateXlibSurface(VkInstance, const gecko::platform::NativeWindowHandle&, VkSurfaceKHR*) noexcept;
#endif

#if defined(GECKO_GRAPHICS_VULKAN_WAYLAND)
VkResult CreateWaylandSurface(VkInstance, const gecko::platform::NativeWindowHandle&, VkSurfaceKHR*) noexcept;
#endif

#if defined(GECKO_GRAPHICS_VULKAN_WIN32)
VkResult CreateWin32Surface(VkInstance, const gecko::platform::NativeWindowHandle&, VkSurfaceKHR*) noexcept;
#endif

// -- Public API ------------------------------------------------------------

Span<const char* const> GetRequiredSurfaceExtensions() noexcept
{
  static const char* const Extensions[] = {
      VK_KHR_SURFACE_EXTENSION_NAME,
#if defined(GECKO_GRAPHICS_VULKAN_XLIB)
      "VK_KHR_xlib_surface",
#endif
#if defined(GECKO_GRAPHICS_VULKAN_WAYLAND)
      "VK_KHR_wayland_surface",
#endif
#if defined(GECKO_GRAPHICS_VULKAN_WIN32)
      "VK_KHR_win32_surface",
#endif
  };
  return {Extensions, sizeof(Extensions) / sizeof(Extensions[0])};
}

VkResult CreateSurface(VkInstance instance, const gecko::platform::NativeWindowHandle& native,
                       VkSurfaceKHR* out) noexcept
{
  using gecko::platform::DisplayBackendKind;

  switch (native.Backend)
  {
#if defined(GECKO_GRAPHICS_VULKAN_XLIB)
  case DisplayBackendKind::Xlib:
    return CreateXlibSurface(instance, native, out);
#endif
#if defined(GECKO_GRAPHICS_VULKAN_WAYLAND)
  case DisplayBackendKind::Wayland:
    return CreateWaylandSurface(instance, native, out);
#endif
#if defined(GECKO_GRAPHICS_VULKAN_WIN32)
  case DisplayBackendKind::Win32:
    return CreateWin32Surface(instance, native, out);
#endif
  default:
    GECKO_ERROR(labels::Vulkan, "VulkanSurface: no surface backend for display kind {}",
                static_cast<i32>(native.Backend));
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }
}

}  // namespace gecko::graphics

#endif
