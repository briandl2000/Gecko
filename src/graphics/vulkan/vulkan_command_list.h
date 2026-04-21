#pragma once

#include "gecko/graphics/command_list.h"
#include "vulkan_device.h"

namespace gecko::graphics {

// ── VulkanCommandList ─────────────────────────────────────────────────────
// Recording begins immediately on construction (vkBeginCommandBuffer).
// VulkanDevice::ExecuteGraphicsCommandList calls FinalizeForSubmit() which
// ends any active render pass, inserts the PRESENT transition if needed,
// and calls vkEndCommandBuffer before submission.

class VulkanCommandList final : public ICommandList
{
public:
  explicit VulkanCommandList(VulkanDevice& device) noexcept;
  ~VulkanCommandList() override;

  VulkanCommandList(const VulkanCommandList&)            = delete("VulkanCommandList is not copyable");
  VulkanCommandList& operator=(const VulkanCommandList&) = delete("VulkanCommandList is not copyable");

  // ── ICommandList ──────────────────────────────────────────────

  bool IsValid() const noexcept override;

  // Render target
  void ClearRenderTarget(const RenderTarget& rt) noexcept override;
  void BindRenderTarget(const RenderTarget& rt) noexcept override;

  // Texture (no-op stubs)
  void CopyTextureToTexture(const Texture&, const Texture&) noexcept override {}
  void BindTexture(u32, const Texture&) noexcept override {}
  void BindTexture(u32, const Texture&, u32) noexcept override {}
  void BindAsRWTexture(u32, const Texture&) noexcept override {}
  void BindAsRWTexture(u32, const Texture&, u32) noexcept override {}

  // Buffer
  void BindVertexBuffer(const Buffer& vertexBuffer) noexcept override;
  void BindIndexBuffer(const Buffer&) noexcept override {}
  void BindConstantBuffer(u32, const Buffer&) noexcept override {}
  void BindStructuredBuffer(u32, const Buffer&) noexcept override {}
  void BindAsRWBuffer(u32, const Buffer&) noexcept override {}
  void SetLocalData(u32, const void*) noexcept override {}

  // Pipeline
  void BindGraphicsPipeline(const GraphicsPipeline& pipeline) noexcept override;
  void BindComputePipeline(const ComputePipeline&) noexcept override {}

  // Draw / dispatch
  void Draw(u32 numIndices) noexcept override;
  void DrawAuto(u32 numVertices) noexcept override;
  void Dispatch(u32, u32, u32) noexcept override {}

  // ── Internal: called by VulkanDevice::Execute* ────────────────

  // Ends active rendering, transitions swapchain image to PRESENT, ends cmd buffer.
  void FinalizeForSubmit() noexcept;

  [[nodiscard]] VkCommandBuffer CommandBuffer() const noexcept { return m_CmdBuffer; }
  [[nodiscard]] bool HasSwapchainData() const noexcept { return m_SwapchainData != nullptr; }
  [[nodiscard]] VkSemaphore* ImageAvailableSemaphore() noexcept;
  [[nodiscard]] VkSemaphore* RenderFinishedSemaphore() noexcept;
  [[nodiscard]] VkFence      InFlightFence() noexcept;

private:
  void EndActiveRendering() noexcept;

  VulkanDevice&        m_Device;
  VkCommandBuffer      m_CmdBuffer     { VK_NULL_HANDLE };

  // Current render target state
  VkImage              m_CurrentImage  { VK_NULL_HANDLE };
  VkImageView          m_CurrentView   { VK_NULL_HANDLE };
  u32                  m_RTWidth       { 0 };
  u32                  m_RTHeight      { 0 };
  bool                 m_Rendering     { false };
  bool                 m_IsSwapchain   { false };

  // Pending clear (set by ClearRenderTarget, consumed by BindRenderTarget)
  bool                 m_PendingClear  { false };
  VkClearColorValue    m_ClearColor    {};

  // Swapchain sync pointers (non-owning)
  VulkanSwapchainData* m_SwapchainData { nullptr };
  u32                  m_SwapchainFrame{ 0 };
};

}  // namespace gecko::graphics
