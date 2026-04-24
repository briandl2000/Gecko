#if defined(GECKO_GRAPHICS_VULKAN)
#pragma once

#include "gecko/graphics/graphics_device.h"
#include "vulkan_types.h"

#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace gecko::graphics {

// ── VulkanDevice ──────────────────────────────────────────────────────────

class VulkanDevice final : public GraphicsDevice
{
public:
  explicit VulkanDevice(const GraphicsDeviceDesc& desc) noexcept;
  ~VulkanDevice() override;

  VulkanDevice(const VulkanDevice&) = delete ("VulkanDevice is not copyable");
  VulkanDevice& operator=(const VulkanDevice&) =
      delete ("VulkanDevice is not copyable");

  // ── Swapchain ─────────────────────────────────────────────────

  Swapchain CreateSwapchain(const ::gecko::platform::NativeWindowHandle& native,
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

  RenderTarget CreateRenderTarget(
      const RenderTargetDesc& desc) noexcept override;
  Buffer CreateVertexBuffer(const VertexBufferDesc& desc) noexcept override;
  Buffer CreateIndexBuffer(const IndexBufferDesc& desc) noexcept override;
  Buffer CreateConstantBuffer(const ConstantBufferDesc& desc) noexcept override;
  Buffer CreateStructuredBuffer(
      const StructuredBufferDesc& desc) noexcept override;
  Texture CreateTexture(const TextureDesc& desc) noexcept override;
  Sampler CreateSampler(const SamplerDesc& desc) noexcept override;
  GraphicsPipeline CreateGraphicsPipeline(
      const GraphicsPipelineDesc& desc) noexcept override;
  ComputePipeline CreateComputePipeline(
      const ComputePipelineDesc& desc) noexcept override;
  QueryPool CreateTimestampQueryPool(
      const QueryPoolDesc& desc) noexcept override;
  u32 ReadTimestamps(const QueryPool& pool, u32 firstQuery,
                     ::std::span<u64> out) noexcept override;

  // ── Data upload ────────────────────────────────────────────────

  void UploadTextureData(Texture& texture,
                         ::std::span<const ::gecko::byte> data, u32 mip,
                         u32 slice) noexcept override;
  void UploadBufferData(Buffer& buffer, ::std::span<const ::gecko::byte> data,
                        u32 offset) noexcept override;

  // ── Internal accessors used by VulkanCommandList ──────────────

  [[nodiscard]] VkDevice Device() const noexcept
  {
    return m_Device;
  }
  [[nodiscard]] VkQueue GraphicsQueue() const noexcept
  {
    return m_GraphicsQueue;
  }
  [[nodiscard]] u32 GraphicsQueueFamily() const noexcept
  {
    return m_GraphicsQueueFamily;
  }
  [[nodiscard]] VkCommandPool GraphicsCommandPool() const noexcept
  {
    return m_GraphicsCommandPool;
  }

  /// Return a command pool owned by the calling thread, lazily creating
  /// one on first use. Use this instead of `GraphicsCommandPool()` when
  /// allocating/freeing/resetting command buffers so that command-list
  /// recording can safely happen on job-system worker threads.
  /// The pool is destroyed when the device is destroyed.
  [[nodiscard]] VkCommandPool AcquireThreadCommandPool() noexcept;

  /// Locked `vkDeviceWaitIdle`. Queues are externally synchronised with
  /// submit, so waiting on device idle needs to share the submit mutex.
  void WaitIdleLocked() noexcept;
  [[nodiscard]] VmaAllocator Allocator() const noexcept
  {
    return m_Allocator;
  }
  [[nodiscard]] VkDescriptorPool DescriptorPool() const noexcept
  {
    return m_DescriptorPool;
  }
  [[nodiscard]] bool HasDebugUtils() const noexcept
  {
    return m_HasDebugUtils;
  }

  /// Apply a VK_EXT_debug_utils object name if validation is enabled.
  void SetObjectName(VkObjectType type, u64 handle,
                     const char* name) const noexcept;

private:
  // ── Helpers ───────────────────────────────────────────────────

  /// Build the swapchain + image views + (re)create sync objects once.
  /// Used by CreateSwapchain and ResizeSwapchain.
  [[nodiscard]] bool BuildSwapchainResources(
      VulkanSwapchainData& data, VkSwapchainKHR oldSwapchain) noexcept;

  void DestroySwapchainResources(VulkanSwapchainData& data,
                                 bool destroySurface) noexcept;

  [[nodiscard]] VkShaderModule CreateShaderModule(
      const ShaderCode& code) noexcept;

  void OneTimeSubmit(void (*record)(VkCommandBuffer, void*),
                     void* ctx) noexcept;

  // ── Vulkan objects ─────────────────────────────────────────────

  VkInstance m_Instance {VK_NULL_HANDLE};
  VkPhysicalDevice m_PhysicalDevice {VK_NULL_HANDLE};
  VkDevice m_Device {VK_NULL_HANDLE};

  VkQueue m_GraphicsQueue {VK_NULL_HANDLE};
  VkQueue m_PresentQueue {VK_NULL_HANDLE};
  u32 m_GraphicsQueueFamily {0};
  u32 m_PresentQueueFamily {0};

  VkCommandPool m_GraphicsCommandPool {VK_NULL_HANDLE};

  // Per-thread command pools for thread-safe command-list recording.
  // Protected by m_ThreadPoolsMutex. Entries are never removed during the
  // device's lifetime — freed together in the destructor.
  ::std::mutex m_ThreadPoolsMutex;
  ::std::unordered_map<::std::thread::id, VkCommandPool> m_ThreadPools;

  // Serialises vkQueueSubmit / vkQueuePresentKHR since Vulkan queues are
  // externally synchronised and may be touched from any thread.
  ::std::mutex m_QueueMutex;

  // Deferred command-list destruction: each Execute* submits with a tracker
  // fence and pushes the owning Unique here. ReapPending() runs each frame
  // and drops entries whose fence is signalled (GPU done → safe to free).
  // This avoids a vkDeviceWaitIdle stall on every command-list teardown.
  struct PendingSubmit
  {
    VkFence Fence;
    Unique<ICommandList> Cmd;
  };
  ::std::mutex m_PendingMutex;
  ::std::vector<PendingSubmit> m_Pending;
  ::std::vector<VkFence> m_FreeFences;

  [[nodiscard]] VkFence AcquireTrackerFence() noexcept;
  void ReleaseTrackerFence(VkFence fence) noexcept;
  void ReapPending() noexcept;
  void DrainPending() noexcept;

  VmaAllocator m_Allocator {VK_NULL_HANDLE};

  VkDescriptorPool m_DescriptorPool {VK_NULL_HANDLE};

  VkDebugUtilsMessengerEXT m_DebugMessenger {VK_NULL_HANDLE};

  f32 m_TimestampPeriodNs {1.0F};
  bool m_HasDebugUtils {false};
  bool m_HasHostQueryReset {false};
  bool m_Valid {false};
};

}  // namespace gecko::graphics
#endif
