#if defined(GECKO_PLATFORM_LINUX)
#define VK_USE_PLATFORM_WAYLAND_KHR 1
#define VK_USE_PLATFORM_XLIB_KHR 1
#elif defined(GECKO_PLATFORM_WINDOWS)
#define VK_USE_PLATFORM_WIN32_KHR 1
#endif

#include "graphics_device.cpp"

#if defined(None)
#undef None
#endif
#if defined(Always)
#undef Always
#endif
#if defined(Success)
#undef Success
#endif

#include "graphics.cpp"
#include "private/null_device.cpp"

#include "vulkan/vulkan_command_list.cpp"
#include "vulkan/vulkan_device.cpp"
#include "vulkan/vulkan_gpu_sampler.cpp"
#include "vulkan/vulkan_surface.cpp"
#include "vulkan/linux/vulkan_wayland_surface.cpp"
#include "vulkan/linux/vulkan_xlib_surface.cpp"
#include "vulkan/win32/vulkan_win32_surface.cpp"
