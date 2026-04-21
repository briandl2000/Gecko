#pragma once

#include "gecko/graphics/graphics_device.h"
#include "gecko/platform/platform_config.h"

// Enable Vulkan platform surface extensions before including Vulkan headers
#if defined(GECKO_PLATFORM_LINUX)
#  define VK_USE_PLATFORM_XLIB_KHR    1
#  define VK_USE_PLATFORM_WAYLAND_KHR 1
#  include <X11/Xlib.h>
#  include <wayland-client.h>
// X11 defines macros that collide with our code — undefine them
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

#define VMA_STATIC_VULKAN_FUNCTIONS  0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 1
#include <vk_mem_alloc.h>

namespace gecko::graphics {

// ── Per-frame swapchain resources ─────────────────────────────────────────

struct VulkanSwapchainData
{
  VkSurfaceKHR    Surface          {VK_NULL_HANDLE};
  VkSwapchainKHR  Swapchain        {VK_NULL_HANDLE};
  u32             ImageCount       {0};
  VkImage         Images[8]        {};
  VkImageView     ImageViews[8]    {};
  VkFormat        Format           {VK_FORMAT_UNDEFINED};
  VkExtent2D      Extent           {};

  // per-frame sync
  VkSemaphore     ImageAvailable[8]{};
  VkSemaphore     RenderFinished[8]{};
  VkFence         InFlight[8]      {};
  u32             FrameIndex       {0};
  u32             AcquiredIndex    {0};
};

// ── Render target GPU data ────────────────────────────────────────────────
// Stored in RenderTarget::Data so VulkanCommandList can read it.

struct VulkanRTData
{
  enum class Kind : u8 { Swapchain, Offscreen };

  Kind        RTKind    { Kind::Offscreen };
  VkImage     Image     { VK_NULL_HANDLE };
  VkImageView ImageView { VK_NULL_HANDLE };

  // Only valid when RTKind == Swapchain
  VulkanSwapchainData* SwapchainData { nullptr };
  u32                  FrameIndex    { 0 };
};

// ── VulkanDevice ─────────────────────────────────────────────────────────

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

  RenderTarget GetCurrentBackBuffer(
      const Swapchain& swapchain) const noexcept override;
  u32 GetCurrentBackBufferIndex(
      const Swapchain& swapchain) const noexcept override;
  void Present(const Swapchain& swapchain) noexcept override;

  // ── Command lists ──────────────────────────────────────────────

  Unique<ICommandList> CreateGraphicsCommandList() noexcept override;
  void ExecuteGraphicsCommandList(
      Unique<ICommandList> commandList) noexcept override;
  Unique<ICommandList> CreateComputeCommandList() noexcept override;
  void ExecuteComputeCommandList(
      Unique<ICommandList> commandList) noexcept override;

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
                          ::std::span<const ::gecko::byte> data,
                          u32 mip, u32 slice) noexcept override;
  void UploadBufferData(Buffer& buffer,
                         ::std::span<const ::gecko::byte> data,
                         u32 offset) noexcept override;

  // ── Internal accessors used by VulkanCommandList ──────────────

  [[nodiscard]] VkDevice       Device()   const noexcept { return m_Device; }
  [[nodiscard]] VkQueue        GraphicsQueue() const noexcept { return m_GraphicsQueue; }
  [[nodiscard]] u32            GraphicsQueueFamily() const noexcept { return m_GraphicsQueueFamily; }
  [[nodiscard]] VkCommandPool  GraphicsCommandPool() const noexcept { return m_GraphicsCommandPool; }
  [[nodiscard]] VmaAllocator   Allocator() const noexcept { return m_Allocator; }

private:
  // ── Helpers ───────────────────────────────────────────────────

  void CreateSwapchainInternal(VulkanSwapchainData& data,
                                const ::gecko::platform::NativeWindowHandle& native,
                                const SwapchainDesc& desc) noexcept;
  void DestroySwapchainInternal(VulkanSwapchainData& data) noexcept;

  [[nodiscard]] VkShaderModule CreateShaderModule(const void* code, usize size) noexcept;
  [[nodiscard]] VkShaderModule LoadShaderModule(const char* path) noexcept;

  void OneTimeSubmit(void (*record)(VkCommandBuffer, void*), void* ctx) noexcept;

  // ── Vulkan objects ─────────────────────────────────────────────

  VkInstance       m_Instance       {VK_NULL_HANDLE};
  VkPhysicalDevice m_PhysicalDevice {VK_NULL_HANDLE};
  VkDevice         m_Device         {VK_NULL_HANDLE};

  VkQueue m_GraphicsQueue       {VK_NULL_HANDLE};
  VkQueue m_PresentQueue        {VK_NULL_HANDLE};
  u32     m_GraphicsQueueFamily {0};
  u32     m_PresentQueueFamily  {0};

  VkCommandPool m_GraphicsCommandPool {VK_NULL_HANDLE};

  VmaAllocator m_Allocator {VK_NULL_HANDLE};

  VkDebugUtilsMessengerEXT m_DebugMessenger {VK_NULL_HANDLE};

  bool m_Valid {false};
};

// ── Buffer GPU data ───────────────────────────────────────────────────────

struct VulkanBufferData
{
  VkBuffer      Buffer    {VK_NULL_HANDLE};
  VmaAllocation Allocation{nullptr};
};

// ── Pipeline GPU data ─────────────────────────────────────────────────────

struct VulkanPipelineData
{
  VkPipelineLayout Layout   {VK_NULL_HANDLE};
  VkPipeline       Pipeline {VK_NULL_HANDLE};
};

}  // namespace gecko::graphics
