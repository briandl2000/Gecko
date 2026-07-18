#if defined(GECKO_PLATFORM_LINUX)
#define VK_USE_PLATFORM_WAYLAND_KHR 1
#define VK_USE_PLATFORM_XLIB_KHR 1
#elif defined(GECKO_PLATFORM_WINDOWS)
#define VK_USE_PLATFORM_WIN32_KHR 1
#endif

#include "gecko/gecko.h"

#include "core/gecko_core.cpp"
#include "runtime/gecko_runtime.cpp"
#include "graphics/gecko_graphics.cpp"
#include "platform/gecko_platform.cpp"

#include "graphics/vulkan/linux/vulkan_xlib_surface.cpp"

#include "gecko.cpp"
