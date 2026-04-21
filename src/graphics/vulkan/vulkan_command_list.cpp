#include "vulkan_command_list.h"
#include "vulkan_util.h"

#include "gecko/core/services/log.h"
#include "private/labels.h"

namespace gecko::graphics {

// ── Construction / destruction ────────────────────────────────────────────

VulkanCommandList::VulkanCommandList(VulkanDevice& device) noexcept
    : m_Device(device)
{
  VkCommandBufferAllocateInfo cbAI{};
  cbAI.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cbAI.commandPool        = m_Device.GraphicsCommandPool();
  cbAI.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cbAI.commandBufferCount = 1;

  if (vkAllocateCommandBuffers(m_Device.Device(), &cbAI, &m_CmdBuffer) != VK_SUCCESS)
  {
    GECKO_ERROR(labels::Graphics, "VulkanCommandList: vkAllocateCommandBuffers failed");
    return;
  }

  // Begin recording immediately.
  VkCommandBufferBeginInfo bi{};
  bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  VULKAN_CHECK(vkBeginCommandBuffer(m_CmdBuffer, &bi));
}

VulkanCommandList::~VulkanCommandList()
{
  if (m_CmdBuffer != VK_NULL_HANDLE)
  {
    vkFreeCommandBuffers(m_Device.Device(),
                          m_Device.GraphicsCommandPool(), 1, &m_CmdBuffer);
    m_CmdBuffer = VK_NULL_HANDLE;
  }
}

bool VulkanCommandList::IsValid() const noexcept
{
  return m_CmdBuffer != VK_NULL_HANDLE;
}

// ── Render target ─────────────────────────────────────────────────────────

void VulkanCommandList::ClearRenderTarget(const RenderTarget& rt) noexcept
{
  m_PendingClear = true;

  if (rt.Desc.NumRenderTargets > 0)
  {
    const auto& cv          = rt.Desc.RenderTargetClearValues[0];
    m_ClearColor.float32[0] = cv.Color[0];
    m_ClearColor.float32[1] = cv.Color[1];
    m_ClearColor.float32[2] = cv.Color[2];
    m_ClearColor.float32[3] = cv.Color[3];
  }
  else
  {
    m_ClearColor = {};  // black
  }
}

void VulkanCommandList::BindRenderTarget(const RenderTarget& rt) noexcept
{
  if (!rt.Data)
    return;

  if (m_Rendering)
    EndActiveRendering();

  auto* rtData    = static_cast<VulkanRTData*>(rt.Data.get());
  m_CurrentImage  = rtData->Image;
  m_CurrentView   = rtData->ImageView;
  m_RTWidth       = rt.Desc.Width;
  m_RTHeight      = rt.Desc.Height;
  m_IsSwapchain   = (rtData->RTKind == VulkanRTData::Kind::Swapchain);

  if (m_IsSwapchain)
  {
    m_SwapchainData  = rtData->SwapchainData;
    m_SwapchainFrame = rtData->FrameIndex;
  }

  // Transition: UNDEFINED -> COLOR_ATTACHMENT_OPTIMAL
  VkImageMemoryBarrier barrier{};
  barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout                       = VK_IMAGE_LAYOUT_UNDEFINED;
  barrier.newLayout                       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
  barrier.image                           = m_CurrentImage;
  barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel   = 0;
  barrier.subresourceRange.levelCount     = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount     = 1;
  barrier.srcAccessMask                   = 0;
  barrier.dstAccessMask                   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  vkCmdPipelineBarrier(m_CmdBuffer,
                        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                        0, 0, nullptr, 0, nullptr, 1, &barrier);

  VkRenderingAttachmentInfo colorAtt{};
  colorAtt.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  colorAtt.imageView   = m_CurrentView;
  colorAtt.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  colorAtt.loadOp      = m_PendingClear
                             ? VK_ATTACHMENT_LOAD_OP_CLEAR
                             : VK_ATTACHMENT_LOAD_OP_LOAD;
  colorAtt.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
  if (m_PendingClear)
    colorAtt.clearValue.color = m_ClearColor;
  m_PendingClear = false;

  VkRenderingInfo renderingInfo{};
  renderingInfo.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
  renderingInfo.renderArea.offset    = {0, 0};
  renderingInfo.renderArea.extent    = {m_RTWidth, m_RTHeight};
  renderingInfo.layerCount           = 1;
  renderingInfo.colorAttachmentCount = 1;
  renderingInfo.pColorAttachments    = &colorAtt;

  vkCmdBeginRendering(m_CmdBuffer, &renderingInfo);
  m_Rendering = true;

  // Dynamic viewport / scissor (pipeline uses VK_DYNAMIC_STATE_*)
  VkViewport vp{};
  vp.x        = 0.0F;
  vp.y        = 0.0F;
  vp.width    = static_cast<float>(m_RTWidth);
  vp.height   = static_cast<float>(m_RTHeight);
  vp.minDepth = 0.0F;
  vp.maxDepth = 1.0F;
  vkCmdSetViewport(m_CmdBuffer, 0, 1, &vp);

  VkRect2D scissor{};
  scissor.offset = {0, 0};
  scissor.extent = {m_RTWidth, m_RTHeight};
  vkCmdSetScissor(m_CmdBuffer, 0, 1, &scissor);
}

// ── Pipeline ──────────────────────────────────────────────────────────────

void VulkanCommandList::BindGraphicsPipeline(const GraphicsPipeline& pipeline) noexcept
{
  if (!pipeline.Data)
    return;
  auto* pipeData = static_cast<VulkanPipelineData*>(pipeline.Data.get());
  vkCmdBindPipeline(m_CmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeData->Pipeline);
}

// ── Buffer ────────────────────────────────────────────────────────────────

void VulkanCommandList::BindVertexBuffer(const Buffer& buf) noexcept
{
  if (!buf.Data)
    return;
  auto*        bufData = static_cast<VulkanBufferData*>(buf.Data.get());
  VkDeviceSize offset  = 0;
  vkCmdBindVertexBuffers(m_CmdBuffer, 0, 1, &bufData->Buffer, &offset);
}

// ── Draw ──────────────────────────────────────────────────────────────────

void VulkanCommandList::DrawAuto(u32 numVertices) noexcept
{
  vkCmdDraw(m_CmdBuffer, numVertices, 1, 0, 0);
}

void VulkanCommandList::Draw(u32 numIndices) noexcept
{
  vkCmdDraw(m_CmdBuffer, numIndices, 1, 0, 0);
}

// ── Finalise ──────────────────────────────────────────────────────────────

void VulkanCommandList::EndActiveRendering() noexcept
{
  if (!m_Rendering)
    return;

  vkCmdEndRendering(m_CmdBuffer);
  m_Rendering = false;

  if (m_IsSwapchain && m_CurrentImage != VK_NULL_HANDLE)
  {
    // Transition: COLOR_ATTACHMENT_OPTIMAL -> PRESENT_SRC_KHR
    VkImageMemoryBarrier barrier{};
    barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout                       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    barrier.newLayout                       = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    barrier.image                           = m_CurrentImage;
    barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel   = 0;
    barrier.subresourceRange.levelCount     = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount     = 1;
    barrier.srcAccessMask                   = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    barrier.dstAccessMask                   = 0;

    vkCmdPipelineBarrier(m_CmdBuffer,
                          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                          VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                          0, 0, nullptr, 0, nullptr, 1, &barrier);
  }

  m_CurrentImage = VK_NULL_HANDLE;
  m_CurrentView  = VK_NULL_HANDLE;
}

void VulkanCommandList::FinalizeForSubmit() noexcept
{
  EndActiveRendering();
  VULKAN_CHECK(vkEndCommandBuffer(m_CmdBuffer));
}

// ── Sync accessors ────────────────────────────────────────────────────────

VkSemaphore* VulkanCommandList::ImageAvailableSemaphore() noexcept
{
  if (!m_SwapchainData)
    return nullptr;
  return &m_SwapchainData->ImageAvailable[m_SwapchainFrame];
}

VkSemaphore* VulkanCommandList::RenderFinishedSemaphore() noexcept
{
  if (!m_SwapchainData)
    return nullptr;
  return &m_SwapchainData->RenderFinished[m_SwapchainFrame];
}

VkFence VulkanCommandList::InFlightFence() noexcept
{
  if (!m_SwapchainData)
    return VK_NULL_HANDLE;
  return m_SwapchainData->InFlight[m_SwapchainFrame];
}

}  // namespace gecko::graphics
