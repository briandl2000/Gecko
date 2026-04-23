#if defined(GECKO_GRAPHICS_VULKAN)
#pragma once

#include "gecko/graphics/graphics_types.h"
#include "gecko/platform/window.h"

#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vk_mem_alloc.h>

// ── Vulkan backend GPU-object payloads ──────────────────────────────────
// These are the structs attached to Swapchain/Texture/RenderTarget/Buffer/
// Pipeline `Data` (via Shared<void>) when the Vulkan backend is active.
// They are strictly internal to the graphics module's Vulkan TUs; no
// public header exposes Vulkan symbols.

namespace gecko::graphics {

// ── Swapchain GPU data ──────────────────────────────────────────────────
// One `VulkanSwapchainData` per Swapchain. Sync objects are sized by
// `MaxFramesInFlight` — decoupled from the driver-chosen `ImageCount`.

struct VulkanSwapchainData
{
  VkSurfaceKHR Surface {VK_NULL_HANDLE};
  VkSwapchainKHR Swapchain {VK_NULL_HANDLE};
  VkFormat Format {VK_FORMAT_UNDEFINED};
  VkExtent2D Extent {};

  u32 ImageCount {0};
  VkImage Images[MaxSwapchainImages] {};
  VkImageView ImageViews[MaxSwapchainImages] {};

  // Per-frame sync (indexed by FrameIndex)
  VkSemaphore ImageAvailable[MaxFramesInFlight] {};
  VkSemaphore RenderFinished[MaxFramesInFlight] {};
  VkFence InFlight[MaxFramesInFlight] {};

  u32 FrameIndex {0};     ///< wraps mod MaxFramesInFlight
  u32 AcquiredIndex {0};  ///< last image returned by vkAcquireNextImageKHR

  ::gecko::platform::NativeWindowHandle Native {};
  SwapchainDesc Desc {};
};

// ── Texture GPU data ────────────────────────────────────────────────────
// Attached to Texture::Data. Owns VkImage + VkImageView + VMA allocation.
// `CurrentLayout` is mutated by the command list as barriers are recorded.

struct VulkanTextureData
{
  VkImage Image {VK_NULL_HANDLE};
  VmaAllocation Allocation {nullptr};
  VkImageView ImageView {VK_NULL_HANDLE};
  VkFormat Format {VK_FORMAT_UNDEFINED};
  u32 Width {0};
  u32 Height {0};
  VkImageLayout CurrentLayout {VK_IMAGE_LAYOUT_UNDEFINED};
  VkImageAspectFlags Aspect {VK_IMAGE_ASPECT_COLOR_BIT};
  bool IsRenderTarget {false};
};

// ── RenderTarget GPU data ───────────────────────────────────────────────
// Attached to RenderTarget::Data. Back buffers are per-image and owned by
// the Swapchain; offscreen RTs reference their own Texture data (which the
// containing RenderTarget holds alive via RenderTextures[]).

struct VulkanRTData
{
  enum class Kind : u8
  {
    Swapchain,
    Offscreen,
  };

  Kind RTKind {Kind::Offscreen};
  VkImage Image {VK_NULL_HANDLE};  ///< primary color image (non-owning alias)
  VkImageView ImageView {
      VK_NULL_HANDLE};  ///< primary color view  (non-owning alias)

  // Only valid when RTKind == Swapchain
  VulkanSwapchainData* SwapchainData {nullptr};
  u32 FrameIndex {0};

  // Only valid when RTKind == Offscreen. Non-owning pointers into the
  // Texture::Data payloads stored on the parent RenderTarget; the command
  // list pokes CurrentLayout through these.
  VulkanTextureData* OffscreenTex[RenderTargetDesc::MaxRenderTargets] {};
  VulkanTextureData* OffscreenDepth {nullptr};
  u32 NumOffscreen {0};
};

// ── Buffer GPU data ─────────────────────────────────────────────────────

struct VulkanBufferData
{
  VkBuffer Buffer {VK_NULL_HANDLE};
  VmaAllocation Allocation {nullptr};
};

// ── Pipeline GPU data ───────────────────────────────────────────────────

struct VulkanPipelineData
{
  VkPipelineLayout Layout {VK_NULL_HANDLE};
  VkPipeline Pipeline {VK_NULL_HANDLE};
  VkDescriptorSetLayout DescSetLayout {VK_NULL_HANDLE};
  // Per-binding descriptor types so BindXxx() can look up what to write.
  static constexpr u32 MaxBindings = 64;
  VkDescriptorType BindingTypes[MaxBindings] {};
  u32 NumBindings {0};
  u32 PushConstantBytes {0};
  bool IsCompute {false};
};

// ── Sampler GPU data ────────────────────────────────────────────────────

struct VulkanSamplerData
{
  VkSampler Sampler {VK_NULL_HANDLE};
};

// ── Query pool GPU data ─────────────────────────────────────────────────

struct VulkanQueryPoolData
{
  VkQueryPool QueryPool {VK_NULL_HANDLE};
  u32 Count {0};
  f32 TimestampPeriodNs {1.0F};
};

}  // namespace gecko::graphics
#endif
