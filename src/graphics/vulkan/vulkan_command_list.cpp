#include "vulkan_command_list.h"
#include "vulkan_device.h"
#include "vulkan_util.h"

#include "gecko/core/services/log.h"
#include "private/labels.h"

namespace gecko::graphics {

VulkanCommandList::VulkanCommandList(VulkanDevice& device, bool compute) noexcept
    : m_Device(&device), m_Compute(compute)
{
  VkCommandBufferAllocateInfo ai{};
  ai.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  ai.commandPool        = device.GraphicsCommandPool();
  ai.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  ai.commandBufferCount = 1;
  VULKAN_CHECK(vkAllocateCommandBuffers(device.Device(), &ai, &m_CmdBuffer));

  // Per-command-list descriptor pool. Reset at Begin(). Sized to handle
  // many BindPipeline calls per frame without running out.
  VkDescriptorPoolSize sizes[] = {
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 128},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,         128},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,          64},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,           32},
  };
  VkDescriptorPoolCreateInfo dpci{};
  dpci.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  dpci.maxSets       = 128;
  dpci.poolSizeCount = sizeof(sizes) / sizeof(sizes[0]);
  dpci.pPoolSizes    = sizes;
  VULKAN_CHECK(
      vkCreateDescriptorPool(device.Device(), &dpci, nullptr, &m_DescPool));
}

VulkanCommandList::~VulkanCommandList()
{
  if (m_Device == nullptr)
    return;
  vkDeviceWaitIdle(m_Device->Device());
  if (m_DescPool != VK_NULL_HANDLE)
    vkDestroyDescriptorPool(m_Device->Device(), m_DescPool, nullptr);
  if (m_CmdBuffer != VK_NULL_HANDLE)
    vkFreeCommandBuffers(m_Device->Device(), m_Device->GraphicsCommandPool(), 1,
                          &m_CmdBuffer);
}

void VulkanCommandList::Begin() noexcept
{
  vkResetCommandBuffer(m_CmdBuffer, 0);
  if (m_DescPool != VK_NULL_HANDLE)
    vkResetDescriptorPool(m_Device->Device(), m_DescPool, 0);
  VkCommandBufferBeginInfo bi{};
  bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  VULKAN_CHECK(vkBeginCommandBuffer(m_CmdBuffer, &bi));

  m_TouchedCount         = 0;
  m_ActiveSwapchainImage = VK_NULL_HANDLE;
  m_ActiveOffscreenRT    = nullptr;
  m_CurrentPipeline      = nullptr;
  m_CurrentDescSet       = VK_NULL_HANDLE;
}

void VulkanCommandList::End() noexcept
{
  // Transition any active swapchain images still in color-attachment layout
  // to PRESENT. The most recent BeginRendering on a swapchain image left it
  // in COLOR_ATTACHMENT_OPTIMAL after EndRendering.
  VULKAN_CHECK(vkEndCommandBuffer(m_CmdBuffer));
}

void VulkanCommandList::TransitionToColorAttachment(VkImage image) noexcept
{
  VkImageMemoryBarrier b{};
  b.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  b.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
  b.newLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  b.image               = image;
  b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  b.subresourceRange.levelCount = 1;
  b.subresourceRange.layerCount = 1;
  b.srcAccessMask       = 0;
  b.dstAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  vkCmdPipelineBarrier(m_CmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0,
                        nullptr, 0, nullptr, 1, &b);
}

void VulkanCommandList::TransitionToPresent(VkImage image) noexcept
{
  VkImageMemoryBarrier b{};
  b.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  b.oldLayout           = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  b.newLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  b.image               = image;
  b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  b.subresourceRange.levelCount = 1;
  b.subresourceRange.layerCount = 1;
  b.srcAccessMask       = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  b.dstAccessMask       = 0;
  vkCmdPipelineBarrier(m_CmdBuffer, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0,
                        nullptr, 1, &b);
}

// Generic two-sided layout transition used for offscreen RTs flipping
// between COLOR_ATTACHMENT and SHADER_READ_ONLY as they're drawn-to and
// sampled within a single command buffer.
void VulkanCommandList::TransitionImage(VkImage            image,
                                         VkImageAspectFlags aspect,
                                         VkImageLayout      oldLayout,
                                         VkImageLayout      newLayout) noexcept
{
  if (oldLayout == newLayout)
    return;

  VkImageMemoryBarrier b{};
  b.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  b.oldLayout           = oldLayout;
  b.newLayout           = newLayout;
  b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  b.image               = image;
  b.subresourceRange.aspectMask = aspect;
  b.subresourceRange.levelCount = 1;
  b.subresourceRange.layerCount = 1;

  VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

  switch (oldLayout)
  {
    case VK_IMAGE_LAYOUT_UNDEFINED:
      b.srcAccessMask = 0;
      srcStage        = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
      break;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      b.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      srcStage        = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      break;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      b.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
      srcStage        = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      break;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      b.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      srcStage        = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
      break;
    default:
      b.srcAccessMask = 0;
      srcStage        = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
      break;
  }

  switch (newLayout)
  {
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      b.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      dstStage        = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      break;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      dstStage        = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
      break;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
      b.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      dstStage        = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
      break;
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
      b.dstAccessMask = 0;
      dstStage        = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
      break;
    default:
      b.dstAccessMask = 0;
      dstStage        = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
      break;
  }

  vkCmdPipelineBarrier(m_CmdBuffer, srcStage, dstStage, 0, 0, nullptr, 0,
                        nullptr, 1, &b);
}

void VulkanCommandList::MaybeRecordSwapchain(const RenderTarget& rt) noexcept
{
  if (!rt.Data)
    return;
  auto* rtd = static_cast<VulkanRTData*>(rt.Data.get());
  if (rtd->RTKind != VulkanRTData::Kind::Swapchain || rtd->SwapchainData == nullptr)
    return;

  // Deduplicate
  for (u32 i = 0; i < m_TouchedCount; ++i)
  {
    if (m_Touched[i].Data == rtd->SwapchainData)
      return;
  }
  if (m_TouchedCount >= MaxSwapchainsPerSubmit)
  {
    GECKO_WARN(labels::Vulkan,
               "VulkanCommandList: more than %u swapchains touched, dropping",
               MaxSwapchainsPerSubmit);
    return;
  }
  m_Touched[m_TouchedCount++] = {rtd->SwapchainData, rtd->FrameIndex};
}

void VulkanCommandList::BeginRendering(const RenderTarget& color,
                                        const ClearValue*   clear) noexcept
{
  if (!color.Data)
    return;
  auto* rtd = static_cast<VulkanRTData*>(color.Data.get());

  MaybeRecordSwapchain(color);

  if (rtd->RTKind == VulkanRTData::Kind::Swapchain)
  {
    TransitionToColorAttachment(rtd->Image);
    m_ActiveSwapchainImage = rtd->Image;
  }
  else
  {
    for (u32 i = 0; i < rtd->NumOffscreen; ++i)
    {
      auto* td = rtd->OffscreenTex[i];
      if (td == nullptr)
        continue;
      TransitionImage(td->Image, td->Aspect, td->CurrentLayout,
                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
      td->CurrentLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }
    m_ActiveOffscreenRT = rtd;
  }

  VkRenderingAttachmentInfo att{};
  att.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  att.imageView   = rtd->ImageView;
  att.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  att.loadOp  = clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
  att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  if (clear != nullptr && clear->Type == ClearValueType::RenderTarget)
  {
    att.clearValue.color.float32[0] = clear->Color[0];
    att.clearValue.color.float32[1] = clear->Color[1];
    att.clearValue.color.float32[2] = clear->Color[2];
    att.clearValue.color.float32[3] = clear->Color[3];
  }

  VkRenderingInfo ri{};
  ri.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
  ri.renderArea.extent    = {color.Desc.Width, color.Desc.Height};
  ri.layerCount           = 1;
  ri.colorAttachmentCount = 1;
  ri.pColorAttachments    = &att;
  vkCmdBeginRendering(m_CmdBuffer, &ri);
}

void VulkanCommandList::BeginRendering(
    ::std::span<const RenderTarget* const> colors,
    const RenderTarget*                    depth,
    ::std::span<const ClearValue>          clears) noexcept
{
  if (colors.empty() && depth == nullptr)
    return;

  VkRenderingAttachmentInfo colorAtts[RenderTargetDesc::MaxRenderTargets]{};
  u32   width  = 0;
  u32   height = 0;

  for (u32 i = 0; i < colors.size(); ++i)
  {
    const RenderTarget* c = colors[i];
    if (c == nullptr || !c->Data)
      continue;
    auto* rtd = static_cast<VulkanRTData*>(c->Data.get());

    MaybeRecordSwapchain(*c);
    if (rtd->RTKind == VulkanRTData::Kind::Swapchain)
    {
      TransitionToColorAttachment(rtd->Image);
      m_ActiveSwapchainImage = rtd->Image;
    }

    colorAtts[i].sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAtts[i].imageView   = rtd->ImageView;
    colorAtts[i].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    const bool hasClear = clears.size() > i;
    colorAtts[i].loadOp  = hasClear ? VK_ATTACHMENT_LOAD_OP_CLEAR
                                     : VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAtts[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    if (hasClear)
    {
      colorAtts[i].clearValue.color.float32[0] = clears[i].Color[0];
      colorAtts[i].clearValue.color.float32[1] = clears[i].Color[1];
      colorAtts[i].clearValue.color.float32[2] = clears[i].Color[2];
      colorAtts[i].clearValue.color.float32[3] = clears[i].Color[3];
    }
    width  = c->Desc.Width;
    height = c->Desc.Height;
  }

  VkRenderingInfo ri{};
  ri.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO;
  ri.renderArea.extent    = {width, height};
  ri.layerCount           = 1;
  ri.colorAttachmentCount = static_cast<u32>(colors.size());
  ri.pColorAttachments    = colorAtts;
  // Depth handling is deferred with offscreen RT implementation.
  (void)depth;
  vkCmdBeginRendering(m_CmdBuffer, &ri);
}

void VulkanCommandList::EndRendering() noexcept
{
  vkCmdEndRendering(m_CmdBuffer);

  if (m_ActiveSwapchainImage != VK_NULL_HANDLE)
  {
    TransitionToPresent(m_ActiveSwapchainImage);
    m_ActiveSwapchainImage = VK_NULL_HANDLE;
  }

  if (m_ActiveOffscreenRT != nullptr)
  {
    // Flip offscreen color attachments to SHADER_READ so subsequent
    // BindTexture within this same command buffer can sample them.
    for (u32 i = 0; i < m_ActiveOffscreenRT->NumOffscreen; ++i)
    {
      auto* td = m_ActiveOffscreenRT->OffscreenTex[i];
      if (td == nullptr)
        continue;
      TransitionImage(td->Image, td->Aspect, td->CurrentLayout,
                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
      td->CurrentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    m_ActiveOffscreenRT = nullptr;
  }
}

void VulkanCommandList::SetViewport(f32 x, f32 y, f32 w, f32 h, f32 minD,
                                     f32 maxD) noexcept
{
  VkViewport vp{};
  vp.x        = x;
  vp.y        = y + h;  // flip Y: Vulkan clip-space is -Y-up; VK_KHR_maintenance1
  vp.width    = w;
  vp.height   = -h;
  vp.minDepth = minD;
  vp.maxDepth = maxD;
  vkCmdSetViewport(m_CmdBuffer, 0, 1, &vp);
}

void VulkanCommandList::SetScissor(i32 x, i32 y, u32 w, u32 h) noexcept
{
  VkRect2D r{};
  r.offset = {x, y};
  r.extent = {w, h};
  vkCmdSetScissor(m_CmdBuffer, 0, 1, &r);
}

namespace {

// Allocate (or reuse) one descriptor set for the currently bound pipeline.
VkDescriptorSet EnsureCurrentDescSet(VulkanCommandList* self,
                                      VkDevice           device,
                                      VkDescriptorPool   pool,
                                      VulkanPipelineData* pd,
                                      VkDescriptorSet&   slot) noexcept
{
  (void)self;
  if (slot != VK_NULL_HANDLE || pd == nullptr
      || pd->DescSetLayout == VK_NULL_HANDLE)
    return slot;
  VkDescriptorSetAllocateInfo ai{};
  ai.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  ai.descriptorPool     = pool;
  ai.descriptorSetCount = 1;
  ai.pSetLayouts        = &pd->DescSetLayout;
  if (vkAllocateDescriptorSets(device, &ai, &slot) != VK_SUCCESS)
  {
    GECKO_ERROR(labels::Vulkan,
                "VulkanCommandList: descriptor-set alloc failed");
    slot = VK_NULL_HANDLE;
  }
  return slot;
}

}  // namespace

void VulkanCommandList::BindPipeline(const GraphicsPipeline& pipeline) noexcept
{
  if (!pipeline.Data)
    return;
  auto* pd = static_cast<VulkanPipelineData*>(pipeline.Data.get());
  vkCmdBindPipeline(m_CmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pd->Pipeline);
  m_CurrentPipeline = pd;
  m_CurrentDescSet  = VK_NULL_HANDLE;  // fresh set on next Bind* call
}

void VulkanCommandList::BindPipeline(const ComputePipeline& pipeline) noexcept
{
  if (!pipeline.Data)
    return;
  auto* pd = static_cast<VulkanPipelineData*>(pipeline.Data.get());
  vkCmdBindPipeline(m_CmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pd->Pipeline);
  m_CurrentPipeline = pd;
  m_CurrentDescSet  = VK_NULL_HANDLE;
}

void VulkanCommandList::BindVertexBuffer(const Buffer& buffer, u32 slot) noexcept
{
  if (!buffer.Data)
    return;
  auto*        bd     = static_cast<VulkanBufferData*>(buffer.Data.get());
  VkDeviceSize offset = 0;
  vkCmdBindVertexBuffers(m_CmdBuffer, slot, 1, &bd->Buffer, &offset);
}

void VulkanCommandList::BindIndexBuffer(const Buffer& buffer) noexcept
{
  if (!buffer.Data)
    return;
  auto* bd = static_cast<VulkanBufferData*>(buffer.Data.get());
  vkCmdBindIndexBuffer(m_CmdBuffer, bd->Buffer, 0, VK_INDEX_TYPE_UINT32);
}

void VulkanCommandList::BindConstantBuffer(u32 slot,
                                            const Buffer& buffer) noexcept
{
  if (m_Device == nullptr || m_CurrentPipeline == nullptr
      || m_CurrentPipeline->DescSetLayout == VK_NULL_HANDLE || !buffer.Data)
    return;
  auto* bd = static_cast<VulkanBufferData*>(buffer.Data.get());
  if (bd->Buffer == VK_NULL_HANDLE)
    return;

  VkDescriptorSet set = EnsureCurrentDescSet(
      this, m_Device->Device(), m_DescPool, m_CurrentPipeline, m_CurrentDescSet);
  if (set == VK_NULL_HANDLE)
    return;

  VkDescriptorBufferInfo bi{};
  bi.buffer = bd->Buffer;
  bi.offset = 0;
  bi.range  = VK_WHOLE_SIZE;

  VkWriteDescriptorSet w{};
  w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  w.dstSet          = set;
  w.dstBinding      = slot;
  w.descriptorCount = 1;
  w.descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  w.pBufferInfo     = &bi;
  vkUpdateDescriptorSets(m_Device->Device(), 1, &w, 0, nullptr);

  const VkPipelineBindPoint bp = m_CurrentPipeline->IsCompute
                                      ? VK_PIPELINE_BIND_POINT_COMPUTE
                                      : VK_PIPELINE_BIND_POINT_GRAPHICS;
  vkCmdBindDescriptorSets(m_CmdBuffer, bp, m_CurrentPipeline->Layout, 0, 1,
                           &set, 0, nullptr);
}

void VulkanCommandList::BindTexture(u32 slot, const Texture& texture) noexcept
{
  if (m_Device == nullptr || m_CurrentPipeline == nullptr
      || m_CurrentPipeline->DescSetLayout == VK_NULL_HANDLE)
    return;
  if (!texture.Data)
    return;
  auto* td = static_cast<VulkanTextureData*>(texture.Data.get());
  if (td->ImageView == VK_NULL_HANDLE)
    return;

  VkDescriptorSet set = EnsureCurrentDescSet(
      this, m_Device->Device(), m_DescPool, m_CurrentPipeline, m_CurrentDescSet);
  if (set == VK_NULL_HANDLE)
    return;

  // Determine descriptor type from the pipeline's binding table.
  VkDescriptorType dtype = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  if (slot < m_CurrentPipeline->NumBindings)
    dtype = m_CurrentPipeline->BindingTypes[slot];

  VkDescriptorImageInfo ii{};
  ii.imageView   = td->ImageView;
  ii.imageLayout = (dtype == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
                       ? VK_IMAGE_LAYOUT_GENERAL
                       : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  ii.sampler     = VK_NULL_HANDLE;  // immutable via DSL when sampled

  VkWriteDescriptorSet w{};
  w.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  w.dstSet          = set;
  w.dstBinding      = slot;
  w.descriptorCount = 1;
  w.descriptorType  = dtype;
  w.pImageInfo      = &ii;
  vkUpdateDescriptorSets(m_Device->Device(), 1, &w, 0, nullptr);

  const VkPipelineBindPoint bp = m_CurrentPipeline->IsCompute
                                      ? VK_PIPELINE_BIND_POINT_COMPUTE
                                      : VK_PIPELINE_BIND_POINT_GRAPHICS;
  vkCmdBindDescriptorSets(m_CmdBuffer, bp, m_CurrentPipeline->Layout, 0, 1,
                           &set, 0, nullptr);
}

void VulkanCommandList::Draw(u32 vertexCount, u32 instanceCount,
                              u32 firstVertex, u32 firstInstance) noexcept
{
  vkCmdDraw(m_CmdBuffer, vertexCount, instanceCount, firstVertex, firstInstance);
}

void VulkanCommandList::DrawIndexed(u32 indexCount, u32 instanceCount,
                                     u32 firstIndex, i32 vertexOffset,
                                     u32 firstInstance) noexcept
{
  vkCmdDrawIndexed(m_CmdBuffer, indexCount, instanceCount, firstIndex,
                    vertexOffset, firstInstance);
}

void VulkanCommandList::Dispatch(u32 x, u32 y, u32 z) noexcept
{
  vkCmdDispatch(m_CmdBuffer, x, y, z);
}

}  // namespace gecko::graphics
