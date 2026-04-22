#pragma once

#include "gecko/graphics/graphics_device.h"

#define VMA_STATIC_VULKAN_FUNCTIONS  0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vk_mem_alloc.h>

namespace gecko::graphics {

// ── Swapchain GPU data ────────────────────────────────────────────────────
// One `VulkanSwapchainData` per Swapchain. Sync objects are sized by
// `MaxFramesInFlight` — decoupled from the driver-chosen `ImageCount`.

struct VulkanSwapchainData
{
  VkSurfaceKHR   Surface {VK_NULL_HANDLE};
  VkSwapchainKHR Swapchain {VK_NULL_HANDLE};
  VkFormat       Format {VK_FORMAT_UNDEFINED};
  VkExtent2D     Extent {};

  u32         ImageCount {0};
  VkImage     Images[MaxSwapchainImages] {};
  VkImageView ImageViews[MaxSwapchainImages] {};

  // Per-frame sync (indexed by FrameIndex)
  VkSemaphore ImageAvailable[MaxFramesInFlight] {};
  VkSemaphore RenderFinished[MaxFramesInFlight] {};
  VkFence     InFlight[MaxFramesInFlight] {};

  u32 FrameIndex {0};     ///< wraps mod MaxFramesInFlight
  u32 AcquiredIndex {0};  ///< last image returned by vkAcquireNextImageKHR

  ::gecko::platform::NativeWindowHandle Native {};
  SwapchainDesc                         Desc {};
};

// ── Texture GPU data ──────────────────────────────────────────────────────
// Attached to Texture::Data. Owns VkImage + VkImageView + VMA allocation.
// `CurrentLayout` is mutated by the command list as barriers are recorded.

struct VulkanTextureData
{
  VkImage            Image {VK_NULL_HANDLE};
  VmaAllocation      Allocation {nullptr};
  VkImageView        ImageView {VK_NULL_HANDLE};
  VkFormat           Format {VK_FORMAT_UNDEFINED};
  u32                Width {0};
  u32                Height {0};
  VkImageLayout      CurrentLayout {VK_IMAGE_LAYOUT_UNDEFINED};
  VkImageAspectFlags Aspect {VK_IMAGE_ASPECT_COLOR_BIT};
  bool               IsRenderTarget {false};
};

// ── RenderTarget GPU data ─────────────────────────────────────────────────
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

  Kind        RTKind {Kind::Offscreen};
  VkImage     Image {VK_NULL_HANDLE};      ///< primary color image (non-owning alias)
  VkImageView ImageView {VK_NULL_HANDLE};  ///< primary color view  (non-owning alias)

  // Only valid when RTKind == Swapchain
  VulkanSwapchainData* SwapchainData {nullptr};
  u32                  FrameIndex {0};

  // Only valid when RTKind == Offscreen. Non-owning pointers into the
  // Texture::Data payloads stored on the parent RenderTarget; the command
  // list pokes CurrentLayout through these.
  VulkanTextureData* OffscreenTex[RenderTargetDesc::MaxRenderTargets] {};
  VulkanTextureData* OffscreenDepth {nullptr};
  u32                NumOffscreen {0};
};

// ── Buffer / pipeline GPU data ────────────────────────────────────────────

struct VulkanBufferData
{
  VkBuffer      Buffer {VK_NULL_HANDLE};
  VmaAllocation Allocation {nullptr};
};

struct VulkanPipelineData
{
  VkPipelineLayout      Layout {VK_NULL_HANDLE};
  VkPipeline            Pipeline {VK_NULL_HANDLE};
  VkDescriptorSetLayout DescSetLayout {VK_NULL_HANDLE};
  VkSampler             Samplers[GraphicsPipelineDesc::MaxSamplers] {};
  u32                   NumSamplers {0};
  u32                   NumTextureBindings {0};
  // Per-binding descriptor types so BindXxx() can look up what to write.
  static constexpr u32  MaxBindings = 64;
  VkDescriptorType      BindingTypes[MaxBindings] {};
  u32                   NumBindings {0};
  bool                  IsCompute {false};
};

// ── VulkanDevice ──────────────────────────────────────────────────────────

class VulkanDevice final : public GraphicsDevice
{
public:
  explicit VulkanDevice(const GraphicsDeviceDesc& desc) noexcept;
  ~VulkanDevice() override;

  VulkanDevice(const VulkanDevice&)            = delete("VulkanDevice is not copyable");
  VulkanDevice& operator=(const VulkanDevice&) = delete("VulkanDevice is not copyable");

  // ── Swapchain ─────────────────────────────────────────────────

  Swapchain CreateSwapchain(
      const ::gecko::platform::NativeWindowHandle& native,
      const SwapchainDesc& desc) noexcept override;
  void DestroySwapchain(Swapchain& swapchain) noexcept override;
  void ResizeSwapchain(Swapchain& swapchain) noexcept override;

  FrameContext BeginFrame(Swapchain& swapchain) noexcept override;
  void Present(::std::span<const FrameContext> frames) noexcept override;

  // ── Command lists ──────────────────────────────────────────────

  Unique<ICommandList> CreateGraphicsCommandList() noexcept override;
  Unique<ICommandList> CreateComputeCommandList() noexcept override;
  void ExecuteGraphicsCommandList(Unique<ICommandList>) noexcept override;
  void ExecuteComputeCommandList(Unique<ICommandList>) noexcept override;

  // ── Resource creation ─────────────────────────────────────────

  RenderTarget     CreateRenderTarget(const RenderTargetDesc& desc) noexcept override;
  Buffer           CreateVertexBuffer(const VertexBufferDesc& desc) noexcept override;
  Buffer           CreateIndexBuffer(const IndexBufferDesc& desc) noexcept override;
  Buffer           CreateConstantBuffer(const ConstantBufferDesc& desc) noexcept override;
  Buffer           CreateStructuredBuffer(const StructuredBufferDesc& desc) noexcept override;
  Texture          CreateTexture(const TextureDesc& desc) noexcept override;
  GraphicsPipeline CreateGraphicsPipeline(const GraphicsPipelineDesc& desc) noexcept override;
  ComputePipeline  CreateComputePipeline(const ComputePipelineDesc& desc) noexcept override;

  // ── Data upload ────────────────────────────────────────────────

  void UploadTextureData(Texture& texture,
                          ::std::span<const ::gecko::byte> data, u32 mip,
                          u32 slice) noexcept override;
  void UploadBufferData(Buffer& buffer,
                         ::std::span<const ::gecko::byte> data,
                         u32 offset) noexcept override;

  // ── Internal accessors used by VulkanCommandList ──────────────

  [[nodiscard]] VkDevice Device() const noexcept { return m_Device; }
  [[nodiscard]] VkQueue  GraphicsQueue() const noexcept { return m_GraphicsQueue; }
  [[nodiscard]] u32      GraphicsQueueFamily() const noexcept { return m_GraphicsQueueFamily; }
  [[nodiscard]] VkCommandPool GraphicsCommandPool() const noexcept { return m_GraphicsCommandPool; }
  [[nodiscard]] VmaAllocator  Allocator() const noexcept { return m_Allocator; }
  [[nodiscard]] VkDescriptorPool DescriptorPool() const noexcept { return m_DescriptorPool; }

private:
  // ── Helpers ───────────────────────────────────────────────────

  /// Build the swapchain + image views + (re)create sync objects once.
  /// Used by CreateSwapchain and ResizeSwapchain.
  [[nodiscard]] bool BuildSwapchainResources(
      VulkanSwapchainData& data, VkSwapchainKHR oldSwapchain) noexcept;

  void DestroySwapchainResources(VulkanSwapchainData& data,
                                  bool destroySurface) noexcept;

  [[nodiscard]] VkShaderModule CreateShaderModule(const ShaderCode& code) noexcept;

  void OneTimeSubmit(void (*record)(VkCommandBuffer, void*), void* ctx) noexcept;

  // ── Vulkan objects ─────────────────────────────────────────────

  VkInstance       m_Instance {VK_NULL_HANDLE};
  VkPhysicalDevice m_PhysicalDevice {VK_NULL_HANDLE};
  VkDevice         m_Device {VK_NULL_HANDLE};

  VkQueue m_GraphicsQueue {VK_NULL_HANDLE};
  VkQueue m_PresentQueue {VK_NULL_HANDLE};
  u32     m_GraphicsQueueFamily {0};
  u32     m_PresentQueueFamily {0};

  VkCommandPool m_GraphicsCommandPool {VK_NULL_HANDLE};

  VmaAllocator m_Allocator {VK_NULL_HANDLE};

  VkDescriptorPool m_DescriptorPool {VK_NULL_HANDLE};

  VkDebugUtilsMessengerEXT m_DebugMessenger {VK_NULL_HANDLE};

  bool m_Valid {false};
};

}  // namespace gecko::graphics
