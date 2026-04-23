#if defined(GECKO_GRAPHICS_VULKAN)
#pragma once

#include "gecko/graphics/graphics_device.h"
#include "vulkan_types.h"

namespace gecko::graphics {

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
#endif
