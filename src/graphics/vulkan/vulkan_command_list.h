#if defined(GECKO_GRAPHICS_VULKAN)
#pragma once

#include "gecko/graphics/command_list.h"

#include <vulkan/vulkan.h>

namespace gecko::graphics {

class VulkanDevice;
struct VulkanSwapchainData;

class VulkanCommandList final : public ICommandList
{
public:
  struct TouchedSwapchain
  {
    VulkanSwapchainData* Data {nullptr};
    u32 FrameIndex {0};
    u32 ImageIndex {0};
  };

  struct TouchedView
  {
    const TouchedSwapchain* Data {nullptr};
    u32 Count {0};
  };

  VulkanCommandList(VulkanDevice& device, bool compute) noexcept;
  ~VulkanCommandList() override;

  VulkanCommandList(const VulkanCommandList&) = delete ("not copyable");
  VulkanCommandList& operator=(const VulkanCommandList&) =
      delete ("not copyable");

  // ── ICommandList API ──────────────────────────────────────────

  void Begin() noexcept override;
  void End() noexcept override;
  [[nodiscard]] bool IsValid() const noexcept override
  {
    return m_CmdBuffer != VK_NULL_HANDLE;
  }

  void BeginRendering(const RenderTarget& color,
                      const ClearValue* clear) noexcept override;
  void BeginRendering(::std::span<const RenderTarget* const> colors,
                      const RenderTarget* depth,
                      ::std::span<const ClearValue> clears) noexcept override;
  void EndRendering() noexcept override;

  void SetViewport(f32 x, f32 y, f32 w, f32 h, f32 minD,
                   f32 maxD) noexcept override;
  void SetScissor(i32 x, i32 y, u32 w, u32 h) noexcept override;

  void BindPipeline(const GraphicsPipeline& pipeline) noexcept override;
  void BindPipeline(const ComputePipeline& pipeline) noexcept override;

  void BindVertexBuffer(const Buffer& buffer, u32 slot) noexcept override;
  void BindIndexBuffer(const Buffer& buffer) noexcept override;
  void BindConstantBuffer(u32 slot, const Buffer& buffer) noexcept override;
  void BindStructuredBuffer(u32 slot, const Buffer& buffer) noexcept override;
  void BindRWStructuredBuffer(u32 slot, const Buffer& buffer) noexcept override;
  void BindTexture(u32 slot, const Texture& texture) noexcept override;
  void BindRWTexture(u32 slot, const Texture& texture) noexcept override;
  void BindSampler(u32 slot, const Sampler& sampler) noexcept override;
  void SetConstants(u32 offset,
                    ::std::span<const ::gecko::byte> bytes) noexcept override;

  void Draw(u32 vertexCount, u32 instanceCount, u32 firstVertex,
            u32 firstInstance) noexcept override;
  void DrawIndexed(u32 indexCount, u32 instanceCount, u32 firstIndex,
                   i32 vertexOffset, u32 firstInstance) noexcept override;
  void DrawIndirect(const Buffer& buffer, u64 offset, u32 drawCount,
                    u32 stride) noexcept override;
  void DrawIndexedIndirect(const Buffer& buffer, u64 offset, u32 drawCount,
                           u32 stride) noexcept override;
  void Dispatch(u32 x, u32 y, u32 z) noexcept override;
  void DispatchIndirect(const Buffer& buffer, u64 offset) noexcept override;

  void TransitionTextureForRead(const Texture& texture) noexcept override;

  void CopyBuffer(const Buffer& dst, u64 dstOffset, const Buffer& src,
                  u64 srcOffset, u64 size) noexcept override;
  void CopyBufferToTexture(const Texture& dst, u32 mip, u32 slice,
                           const Buffer& src, u64 srcOffset) noexcept override;
  void CopyTextureToBuffer(const Buffer& dst, u64 dstOffset, const Texture& src,
                           u32 mip, u32 slice) noexcept override;

  void ResetTimestamps(const QueryPool& pool, u32 first,
                       u32 count) noexcept override;
  void WriteTimestamp(const QueryPool& pool, u32 index) noexcept override;

  // ── Accessors used by VulkanDevice::Execute ───────────────────

  [[nodiscard]] VkCommandBuffer CommandBuffer() const noexcept
  {
    return m_CmdBuffer;
  }
  [[nodiscard]] TouchedView TouchedSwapchains() const noexcept
  {
    return {m_Touched, m_TouchedCount};
  }

private:
  void MaybeRecordSwapchain(const RenderTarget& rt) noexcept;
  void TransitionToColorAttachment(VulkanSwapchainData* data,
                                   u32 imageIndex) noexcept;
  void TransitionToPresent(VulkanSwapchainData* data, u32 imageIndex) noexcept;
  void TransitionImage(VkImage image, VkImageAspectFlags aspect,
                       VkImageLayout oldLayout, VkImageLayout newLayout,
                       u32 baseMip = 0, u32 mipCount = VK_REMAINING_MIP_LEVELS,
                       u32 baseLayer = 0,
                       u32 layerCount = VK_REMAINING_ARRAY_LAYERS) noexcept;

  VulkanDevice* m_Device {nullptr};
  VkCommandBuffer m_CmdBuffer {VK_NULL_HANDLE};
  // Pool this command buffer was allocated from. Captured at construction
  // because command buffers must be freed back to the same pool they came
  // from, and the recording thread may differ from the destruction thread.
  VkCommandPool m_Pool {VK_NULL_HANDLE};
  bool m_Compute {false};

  TouchedSwapchain m_Touched[MaxSwapchainsPerSubmit] {};
  u32 m_TouchedCount {0};

  // Track the single-RT rendering in-progress for proper layout transitions.
  VulkanSwapchainData* m_ActiveSwapchain {nullptr};
  u32 m_ActiveSwapchainImageIndex {0};
  struct VulkanRTData* m_ActiveOffscreenRT {nullptr};

  // Pipeline currently bound for graphics; used by BindTexture to source
  // the descriptor-set layout for ad-hoc descriptor sets.
  struct VulkanPipelineData* m_CurrentPipeline {nullptr};

  // Per-command-list descriptor pool, reset on Begin().
  VkDescriptorPool m_DescPool {VK_NULL_HANDLE};

  // Descriptor set allocated for the currently bound pipeline; shared by
  // all BindTexture / BindConstantBuffer / BindStructuredBuffer calls.
  VkDescriptorSet m_CurrentDescSet {VK_NULL_HANDLE};
};

}  // namespace gecko::graphics
#endif
