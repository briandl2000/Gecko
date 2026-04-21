#include "vulkan_command_list.h"
#include "vulkan_device.h"
#include "vulkan_util.h"

#include "gecko/core/services/log.h"
#include "private/labels.h"

namespace gecko::graphics {

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
  }
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

void VulkanCommandList::Begin() noexcept
{
  VkCommandBufferBeginInfo bi{};
  bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  VULKAN_CHECK(vkBeginCommandBuffer(m_CmdBuffer, &bi));
}

void VulkanCommandList::End() noexcept
{
  VULKAN_CHECK(vkEndCommandBuffer(m_CmdBuffer));
}

void VulkanCommandList::BeginRendering(const RenderTarget& rt,
                                        bool clearColor,
                                        bool /*clearDepth*/) noexcept
{
  if (!rt.Data)
    return;

  auto* bbData = static_cast<BackBufferData*>(rt.Data.get());
  m_CurrentImage     = bbData->Image;
  m_CurrentImageView = bbData->ImageView;
  m_Rendering        = true;

  // Transition: UNDEFINED → COLOR_ATTACHMENT_OPTIMAL
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
  colorAtt.imageView   = m_CurrentImageView;
  colorAtt.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  colorAtt.loadOp      = clearColor ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
  colorAtt.storeOp     = VK_ATTACHMENT_STORE_OP_STORE;
  if (clearColor)
  {
    colorAtt.clearValue.color = {
        {rt.Desc.RenderTargetClearValues[0].Color[0],
         rt.Desc.RenderTargetClearValues[0].Color[1],
         rt.Desc.RenderTargetClearValues[0].Color[2],
         rt.Desc.RenderTargetClearValues[0].Color[3]}};
  }

  VkRenderingInfo renderingInfo{};
  renderingInfo.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
  renderingInfo.renderArea.offset    = {0, 0};
  renderingInfo.renderArea.extent    = {rt.Desc.Width, rt.Desc.Height};
  renderingInfo.layerCount           = 1;
  renderingInfo.colorAttachmentCount = 1;
  renderingInfo.pColorAttachments    = &colorAtt;

  vkCmdBeginRendering(m_CmdBuffer, &renderingInfo);
}

void VulkanCommandList::EndRendering() noexcept
{
  vkCmdEndRendering(m_CmdBuffer);
  m_Rendering = false;

  if (m_CurrentImage == VK_NULL_HANDLE)
    return;

  // Transition: COLOR_ATTACHMENT_OPTIMAL → PRESENT_SRC_KHR
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

  m_CurrentImage     = VK_NULL_HANDLE;
  m_CurrentImageView = VK_NULL_HANDLE;
}

void VulkanCommandList::SetViewport(f32 x, f32 y, f32 width, f32 height,
                                     f32 minDepth, f32 maxDepth) noexcept
{
  VkViewport vp{};
  vp.x        = x;
  vp.y        = y;
  vp.width    = width;
  vp.height   = height;
  vp.minDepth = minDepth;
  vp.maxDepth = maxDepth;
  vkCmdSetViewport(m_CmdBuffer, 0, 1, &vp);
}

void VulkanCommandList::SetScissor(u32 x, u32 y, u32 width, u32 height) noexcept
{
  VkRect2D scissor{};
  scissor.offset = {static_cast<i32>(x), static_cast<i32>(y)};
  scissor.extent = {width, height};
  vkCmdSetScissor(m_CmdBuffer, 0, 1, &scissor);
}

void VulkanCommandList::BindPipeline(const GraphicsPipeline& pipeline) noexcept
{
  if (!pipeline.Data)
    return;
  auto* pipeData = static_cast<VulkanPipelineData*>(pipeline.Data.get());
  vkCmdBindPipeline(m_CmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeData->Pipeline);
}

void VulkanCommandList::DrawVertices(u32 vertexCount, u32 instanceCount,
                                      u32 firstVertex, u32 firstInstance) noexcept
{
  vkCmdDraw(m_CmdBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

void VulkanCommandList::DrawIndexedVertices(u32 indexCount, u32 instanceCount,
                                             u32 firstIndex, i32 vertexOffset,
                                             u32 firstInstance) noexcept
{
  vkCmdDrawIndexed(m_CmdBuffer, indexCount, instanceCount, firstIndex, vertexOffset,
                    firstInstance);
}

// ── Swapchain semaphore accessors ─────────────────────────────────────────

VkSemaphore* VulkanCommandList::ImageAvailableSemaphore() noexcept
{
  if (!m_SwapchainData)
    return nullptr;
  return &m_SwapchainData->ImageAvailable[m_SwapchainData->FrameIndex];
}

VkSemaphore* VulkanCommandList::RenderFinishedSemaphore() noexcept
{
  if (!m_SwapchainData)
    return nullptr;
  return &m_SwapchainData->RenderFinished[m_SwapchainData->FrameIndex];
}

VkFence VulkanCommandList::InFlightFence() noexcept
{
  if (!m_SwapchainData)
    return VK_NULL_HANDLE;
  return m_SwapchainData->InFlight[m_SwapchainData->FrameIndex];
}

// ── Legacy no-op stubs ────────────────────────────────────────────────────

void VulkanCommandList::ClearRenderTarget(const RenderTarget&) noexcept {}
void VulkanCommandList::BindRenderTarget(const RenderTarget&) noexcept {}
void VulkanCommandList::CopyTextureToTexture(const Texture&, const Texture&) noexcept {}
void VulkanCommandList::BindTexture(u32, const Texture&) noexcept {}
void VulkanCommandList::BindTexture(u32, const Texture&, u32) noexcept {}
void VulkanCommandList::BindAsRWTexture(u32, const Texture&) noexcept {}
void VulkanCommandList::BindAsRWTexture(u32, const Texture&, u32) noexcept {}

void VulkanCommandList::BindVertexBuffer(const Buffer& buf) noexcept
{
  if (!buf.Data)
    return;
  auto* bufData = static_cast<VulkanBufferData*>(buf.Data.get());
  VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(m_CmdBuffer, 0, 1, &bufData->Buffer, &offset);
}

void VulkanCommandList::BindIndexBuffer(const Buffer& /*buf*/) noexcept {}
void VulkanCommandList::BindConstantBuffer(u32, const Buffer&) noexcept {}
void VulkanCommandList::BindStructuredBuffer(u32, const Buffer&) noexcept {}
void VulkanCommandList::BindAsRWBuffer(u32, const Buffer&) noexcept {}
void VulkanCommandList::SetLocalData(u32, const void*) noexcept {}

void VulkanCommandList::BindGraphicsPipeline(const GraphicsPipeline& pipeline) noexcept
{
  BindPipeline(pipeline);
}

void VulkanCommandList::BindComputePipeline(const ComputePipeline&) noexcept {}

void VulkanCommandList::Draw(u32 numIndices) noexcept
{
  vkCmdDraw(m_CmdBuffer, numIndices, 1, 0, 0);
}

void VulkanCommandList::DrawAuto(u32 numVertices) noexcept
{
  vkCmdDraw(m_CmdBuffer, numVertices, 1, 0, 0);
}

void VulkanCommandList::Dispatch(u32, u32, u32) noexcept {}

}  // namespace gecko::graphics
