// VMA implementation TU — third-party code, disable strict warnings

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#pragma GCC diagnostic ignored "-Wunused-parameter"

#if defined(GECKO_PLATFORM_LINUX)
#  define VK_USE_PLATFORM_XLIB_KHR    1
#  define VK_USE_PLATFORM_WAYLAND_KHR 1
#  include <X11/Xlib.h>
#  include <wayland-client.h>
#  undef None
#  undef Status
#  undef Bool
#  undef True
#  undef False
#  undef Expose
#  undef DestroyNotify
#elif defined(GECKO_PLATFORM_WINDOWS)
#  define VK_USE_PLATFORM_WIN32_KHR   1
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <windows.h>
#endif

#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS  0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1

#include <vk_mem_alloc.h>

#pragma GCC diagnostic pop
