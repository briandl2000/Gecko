#if defined(GECKO_GRAPHICS_VULKAN)
#pragma once

#include "gecko/platform/window.h"

#include <vulkan/vulkan.h>

namespace gecko::graphics {

// -- Platform surface creation ---------------------------------------------
//
// Implemented per-OS in `vulkan/<os>/vulkan_*_surface.cpp`. The
// The unity source includes only the implementation for the active platform.

/// Returns the instance extensions required by the enabled surface backends
/// (always includes VK_KHR_surface). Pointer lifetime is static.
[[nodiscard]] Span<const char* const> GetRequiredSurfaceExtensions() noexcept;

/// Create a `VkSurfaceKHR` for the given native window. Returns VK_SUCCESS
/// on success and writes the surface to `*out`.
[[nodiscard]] VkResult CreateSurface(VkInstance instance, const gecko::platform::NativeWindowHandle& native,
                                     VkSurfaceKHR* out) noexcept;

}  // namespace gecko::graphics
#endif
