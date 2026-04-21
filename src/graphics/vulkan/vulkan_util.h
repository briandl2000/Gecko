#pragma once

#include "gecko/core/types.h"
#include "gecko/graphics/objects.h"
#include "private/labels.h"

// NOTE: vulkan.h is pulled in transitively via vulkan_device.h / vk_mem_alloc.h
// Do NOT include it here to avoid ordering issues with platform surface headers.
#include <vulkan/vulkan.h>

namespace gecko::graphics {

// ── VkFormat conversion ───────────────────────────────────────────────────

[[nodiscard]] inline VkFormat ToVkFormat(DataFormat fmt) noexcept
{
  switch (fmt)
  {
    case DataFormat::R8G8B8A8_SRGB:          return VK_FORMAT_R8G8B8A8_SRGB;
    case DataFormat::R8G8B8A8_UNORM:         return VK_FORMAT_R8G8B8A8_UNORM;
    case DataFormat::R32G32_FLOAT:           return VK_FORMAT_R32G32_SFLOAT;
    case DataFormat::R32G32B32_FLOAT:        return VK_FORMAT_R32G32B32_SFLOAT;
    case DataFormat::R32G32B32A32_FLOAT:     return VK_FORMAT_R32G32B32A32_SFLOAT;
    case DataFormat::R16G16B16A16_FLOAT:     return VK_FORMAT_R16G16B16A16_SFLOAT;
    case DataFormat::R32_FLOAT:              return VK_FORMAT_R32_SFLOAT;
    case DataFormat::R8_UINT:                return VK_FORMAT_R8_UINT;
    case DataFormat::R16_UINT:               return VK_FORMAT_R16_UINT;
    case DataFormat::R32_UINT:               return VK_FORMAT_R32_UINT;
    case DataFormat::R8_INT:                 return VK_FORMAT_R8_SINT;
    case DataFormat::R16_INT:                return VK_FORMAT_R16_SINT;
    case DataFormat::R32_INT:                return VK_FORMAT_R32_SINT;
    default:                                 return VK_FORMAT_UNDEFINED;
  }
}

[[nodiscard]] inline DataFormat FromVkFormat(VkFormat fmt) noexcept
{
  switch (fmt)
  {
    case VK_FORMAT_R8G8B8A8_SRGB:           return DataFormat::R8G8B8A8_SRGB;
    case VK_FORMAT_B8G8R8A8_SRGB:           return DataFormat::R8G8B8A8_SRGB;
    case VK_FORMAT_R8G8B8A8_UNORM:          return DataFormat::R8G8B8A8_UNORM;
    case VK_FORMAT_B8G8R8A8_UNORM:          return DataFormat::R8G8B8A8_UNORM;
    default:                                return DataFormat::None;
  }
}

// ── Error-check macro ─────────────────────────────────────────────────────

#define VULKAN_CHECK(expr)                                                    \
  do {                                                                        \
    VkResult _vkr = (expr);                                                   \
    if (_vkr != VK_SUCCESS)                                                   \
    {                                                                         \
      GECKO_ERROR(::gecko::graphics::labels::Graphics,                       \
                  "Vulkan error %d in %s: %s",                               \
                  static_cast<::gecko::i32>(_vkr), __func__, #expr);         \
    }                                                                         \
  } while (false)

}  // namespace gecko::graphics
