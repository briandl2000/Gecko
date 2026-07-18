#if defined(GECKO_GRAPHICS_VULKAN)
#pragma once

#include "../private/labels.h"
#include "gecko/core/types.h"
#include "gecko/graphics/graphics_types.h"

#include <vulkan/vulkan.h>

namespace gecko::graphics {

[[nodiscard]] inline VkFormat ToVkFormat(DataFormat fmt) noexcept
{
  switch (fmt)
  {
  case DataFormat::R8G8B8A8_SRGB:
    return VK_FORMAT_R8G8B8A8_SRGB;
  case DataFormat::R8G8B8A8_UNORM:
    return VK_FORMAT_R8G8B8A8_UNORM;
  case DataFormat::B8G8R8A8_UNORM:
    return VK_FORMAT_B8G8R8A8_UNORM;
  case DataFormat::B8G8R8A8_SRGB:
    return VK_FORMAT_B8G8R8A8_SRGB;
  case DataFormat::R32G32_FLOAT:
    return VK_FORMAT_R32G32_SFLOAT;
  case DataFormat::R32G32B32_FLOAT:
    return VK_FORMAT_R32G32B32_SFLOAT;
  case DataFormat::R32G32B32A32_FLOAT:
    return VK_FORMAT_R32G32B32A32_SFLOAT;
  case DataFormat::R16G16B16A16_FLOAT:
    return VK_FORMAT_R16G16B16A16_SFLOAT;
  case DataFormat::R32_FLOAT:
    return VK_FORMAT_R32_SFLOAT;
  case DataFormat::R8_UINT:
    return VK_FORMAT_R8_UINT;
  case DataFormat::R16_UINT:
    return VK_FORMAT_R16_UINT;
  case DataFormat::R32_UINT:
    return VK_FORMAT_R32_UINT;
  case DataFormat::R8_INT:
    return VK_FORMAT_R8_SINT;
  case DataFormat::R16_INT:
    return VK_FORMAT_R16_SINT;
  case DataFormat::R32_INT:
    return VK_FORMAT_R32_SINT;
  case DataFormat::D32_FLOAT:
    return VK_FORMAT_D32_SFLOAT;
  case DataFormat::D24_UNORM_S8_UINT:
    return VK_FORMAT_D24_UNORM_S8_UINT;
  case DataFormat::D16_UNORM:
    return VK_FORMAT_D16_UNORM;
  default:
    return VK_FORMAT_UNDEFINED;
  }
}

[[nodiscard]] inline DataFormat FromVkFormat(VkFormat fmt) noexcept
{
  switch (fmt)
  {
  case VK_FORMAT_R8G8B8A8_SRGB:
    return DataFormat::R8G8B8A8_SRGB;
  case VK_FORMAT_R8G8B8A8_UNORM:
    return DataFormat::R8G8B8A8_UNORM;
  case VK_FORMAT_B8G8R8A8_UNORM:
    return DataFormat::B8G8R8A8_UNORM;
  case VK_FORMAT_B8G8R8A8_SRGB:
    return DataFormat::B8G8R8A8_SRGB;
  case VK_FORMAT_R32G32_SFLOAT:
    return DataFormat::R32G32_FLOAT;
  case VK_FORMAT_R32G32B32_SFLOAT:
    return DataFormat::R32G32B32_FLOAT;
  case VK_FORMAT_R32G32B32A32_SFLOAT:
    return DataFormat::R32G32B32A32_FLOAT;
  case VK_FORMAT_R16G16B16A16_SFLOAT:
    return DataFormat::R16G16B16A16_FLOAT;
  case VK_FORMAT_R32_SFLOAT:
    return DataFormat::R32_FLOAT;
  case VK_FORMAT_D32_SFLOAT:
    return DataFormat::D32_FLOAT;
  case VK_FORMAT_D24_UNORM_S8_UINT:
    return DataFormat::D24_UNORM_S8_UINT;
  case VK_FORMAT_D16_UNORM:
    return DataFormat::D16_UNORM;
  default:
    return DataFormat::None;
  }
}

#define VULKAN_CHECK(expr)                                                                               \
  do                                                                                                     \
  {                                                                                                      \
    VkResult _vkr = (expr);                                                                              \
    if (_vkr != VK_SUCCESS)                                                                              \
    {                                                                                                    \
      GECKO_ERROR(labels::Vulkan, "Vulkan error {} in {}: {}", static_cast<i32>(_vkr), __func__, #expr); \
    }                                                                                                    \
  } while (false)

[[nodiscard]] inline VkCompareOp ToVkCompareOp(CompareFunc f) noexcept
{
  switch (f)
  {
  case CompareFunc::Never:
    return VK_COMPARE_OP_NEVER;
  case CompareFunc::Less:
    return VK_COMPARE_OP_LESS;
  case CompareFunc::Equal:
    return VK_COMPARE_OP_EQUAL;
  case CompareFunc::LessEqual:
    return VK_COMPARE_OP_LESS_OR_EQUAL;
  case CompareFunc::Greater:
    return VK_COMPARE_OP_GREATER;
  case CompareFunc::NotEqual:
    return VK_COMPARE_OP_NOT_EQUAL;
  case CompareFunc::GreaterEqual:
    return VK_COMPARE_OP_GREATER_OR_EQUAL;
  case CompareFunc::Always:
    return VK_COMPARE_OP_ALWAYS;
  }
  return VK_COMPARE_OP_ALWAYS;
}

[[nodiscard]] inline VkStencilOp ToVkStencilOp(StencilOp op) noexcept
{
  switch (op)
  {
  case StencilOp::Keep:
    return VK_STENCIL_OP_KEEP;
  case StencilOp::Zero:
    return VK_STENCIL_OP_ZERO;
  case StencilOp::Replace:
    return VK_STENCIL_OP_REPLACE;
  case StencilOp::IncrementClamp:
    return VK_STENCIL_OP_INCREMENT_AND_CLAMP;
  case StencilOp::DecrementClamp:
    return VK_STENCIL_OP_DECREMENT_AND_CLAMP;
  case StencilOp::Invert:
    return VK_STENCIL_OP_INVERT;
  case StencilOp::IncrementWrap:
    return VK_STENCIL_OP_INCREMENT_AND_WRAP;
  case StencilOp::DecrementWrap:
    return VK_STENCIL_OP_DECREMENT_AND_WRAP;
  }
  return VK_STENCIL_OP_KEEP;
}

[[nodiscard]] inline VkBlendFactor ToVkBlendFactor(BlendFactor f) noexcept
{
  switch (f)
  {
  case BlendFactor::Zero:
    return VK_BLEND_FACTOR_ZERO;
  case BlendFactor::One:
    return VK_BLEND_FACTOR_ONE;
  case BlendFactor::SrcColor:
    return VK_BLEND_FACTOR_SRC_COLOR;
  case BlendFactor::InvSrcColor:
    return VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR;
  case BlendFactor::SrcAlpha:
    return VK_BLEND_FACTOR_SRC_ALPHA;
  case BlendFactor::InvSrcAlpha:
    return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
  case BlendFactor::DstColor:
    return VK_BLEND_FACTOR_DST_COLOR;
  case BlendFactor::InvDstColor:
    return VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;
  case BlendFactor::DstAlpha:
    return VK_BLEND_FACTOR_DST_ALPHA;
  case BlendFactor::InvDstAlpha:
    return VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
  case BlendFactor::SrcAlphaSaturate:
    return VK_BLEND_FACTOR_SRC_ALPHA_SATURATE;
  }
  return VK_BLEND_FACTOR_ZERO;
}

[[nodiscard]] inline VkBlendOp ToVkBlendOp(BlendOp op) noexcept
{
  switch (op)
  {
  case BlendOp::Add:
    return VK_BLEND_OP_ADD;
  case BlendOp::Subtract:
    return VK_BLEND_OP_SUBTRACT;
  case BlendOp::ReverseSubtract:
    return VK_BLEND_OP_REVERSE_SUBTRACT;
  case BlendOp::Min:
    return VK_BLEND_OP_MIN;
  case BlendOp::Max:
    return VK_BLEND_OP_MAX;
  }
  return VK_BLEND_OP_ADD;
}

[[nodiscard]] inline VkColorComponentFlags ToVkColorWriteMask(u8 mask) noexcept
{
  VkColorComponentFlags out = 0;
  if (mask & static_cast<u8>(ColorWriteMask::Red))
    out |= VK_COLOR_COMPONENT_R_BIT;
  if (mask & static_cast<u8>(ColorWriteMask::Green))
    out |= VK_COLOR_COMPONENT_G_BIT;
  if (mask & static_cast<u8>(ColorWriteMask::Blue))
    out |= VK_COLOR_COMPONENT_B_BIT;
  if (mask & static_cast<u8>(ColorWriteMask::Alpha))
    out |= VK_COLOR_COMPONENT_A_BIT;
  return out;
}

[[nodiscard]] inline VkShaderStageFlags ToVkShaderStage(ShaderType s) noexcept
{
  switch (s)
  {
  case ShaderType::Vertex:
    return VK_SHADER_STAGE_VERTEX_BIT;
  case ShaderType::Pixel:
    return VK_SHADER_STAGE_FRAGMENT_BIT;
  case ShaderType::Compute:
    return VK_SHADER_STAGE_COMPUTE_BIT;
  case ShaderType::All:
    return VK_SHADER_STAGE_ALL;
  }
  return VK_SHADER_STAGE_ALL;
}

[[nodiscard]] inline VkDescriptorType ToVkDescriptorType(ResourceType t, bool isCompute) noexcept
{
  (void)isCompute;
  switch (t)
  {
  case ResourceType::Texture:
    return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  case ResourceType::RWTexture:
    return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  case ResourceType::ConstantBuffer:
    return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  case ResourceType::StructuredBuffer:
    return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  case ResourceType::RWStructuredBuffer:
    return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  case ResourceType::Sampler:
    return VK_DESCRIPTOR_TYPE_SAMPLER;
  default:
    return VK_DESCRIPTOR_TYPE_MAX_ENUM;
  }
}

}  // namespace gecko::graphics
#endif
