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
#include "vulkan/win32/vulkan_win32_surface.cpp"
