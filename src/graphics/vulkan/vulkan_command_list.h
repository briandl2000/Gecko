#pragma once

#include "gecko/graphics/command_list.h"
#include "vulkan_device.h"

#include <vulkan/vulkan.h>

namespace gecko::graphics {

// ── BackBufferData forward ────────────────────────────────────────────────
// Mirrors the anonymous struct in vulkan_device.cpp so we can read it from
// a RenderTarget's Data pointer.
struct BackBufferData
{
  VkImage     Image;
  VkImageView ImageView;
};

// ── VulkanCommandList ─────────────────────────────────────────────────────

class VulkanCommandList final : public ICommandList
{
public:
  explicit VulkanCommandList(VulkanDevice& device) noexcept;
  ~VulkanCommandList() override;

  VulkanCommandList(const VulkanCommandList&)            = delete("VulkanCommandList is not copyable");
  VulkanCommandList& operator=(const VulkanCommandList&) = delete("VulkanCommandList is not copyable");

  // ── ICommandList ──────────────────────────────────────────────

  bool IsValid() const noexcept override;

  void Begin() noexcept override;
  void End() noexcept override;

  void BeginRendering(const RenderTarget& rt,
                       bool clearColor, bool clearDepth) noexcept override;
  void EndRendering() noexcept override;

  void SetViewport(f32 x, f32 y, f32 width, f32 height,
                    f32 minDepth, f32 maxDepth) noexcept override;
  void SetScissor(u32 x, u32 y, u32 width, u32 height) noexcept override;

  void BindPipeline(const GraphicsPipeline& pipeline) noexcept override;
  void DrawVertices(u32 vertexCount, u32 instanceCount,
                     u32 firstVertex, u32 firstInstance) noexcept override;
  void DrawIndexedVertices(u32 indexCount, u32 instanceCount,
                            u32 firstIndex, i32 vertexOffset,
                            u32 firstInstance) noexcept override;

  // ── Legacy stubs (required by ICommandList) ───────────────────

  void ClearRenderTarget(const RenderTarget&) noexcept override;
  void BindRenderTarget(const RenderTarget&) noexcept override;
  void CopyTextureToTexture(const Texture&, const Texture&) noexcept override;
  void BindTexture(u32, const Texture&) noexcept override;
  void BindTexture(u32, const Texture&, u32) noexcept override;
  void BindAsRWTexture(u32, const Texture&) noexcept override;
  void BindAsRWTexture(u32, const Texture&, u32) noexcept override;
  void BindVertexBuffer(const Buffer& vertexBuffer) noexcept override;
  void BindIndexBuffer(const Buffer& indexBuffer) noexcept override;
  void BindConstantBuffer(u32, const Buffer&) noexcept override;
  void BindStructuredBuffer(u32, const Buffer&) noexcept override;
  void BindAsRWBuffer(u32, const Buffer&) noexcept override;
  void SetLocalData(u32, const void*) noexcept override;
  void BindGraphicsPipeline(const GraphicsPipeline& pipeline) noexcept override;
  void BindComputePipeline(const ComputePipeline&) noexcept override;
  void Draw(u32 numIndices) noexcept override;
  void DrawAuto(u32 numVertices) noexcept override;
  void Dispatch(u32, u32, u32) noexcept override;

  // ── Accessors used by VulkanDevice::Execute* ──────────────────

  [[nodiscard]] VkCommandBuffer CommandBuffer() const noexcept { return m_CmdBuffer; }
  [[nodiscard]] bool HasSwapchainData() const noexcept { return m_SwapchainData != nullptr; }
  [[nodiscard]] VkSemaphore* ImageAvailableSemaphore() noexcept;
  [[nodiscard]] VkSemaphore* RenderFinishedSemaphore() noexcept;
  [[nodiscard]] VkFence      InFlightFence() noexcept;

  void AttachSwapchain(VulkanSwapchainData* data) noexcept { m_SwapchainData = data; }

private:
  VulkanDevice&        m_Device;
  VkCommandBuffer      m_CmdBuffer      {VK_NULL_HANDLE};
  VulkanSwapchainData* m_SwapchainData  {nullptr};

  // Current rendering image (for layout transitions)
  VkImage              m_CurrentImage   {VK_NULL_HANDLE};
  VkImageView          m_CurrentImageView {VK_NULL_HANDLE};
  bool                 m_Rendering      {false};
};

}  // namespace gecko::graphics
