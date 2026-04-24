#if defined(GECKO_GRAPHICS_VULKAN)
#include "vulkan_command_list.h"

#include "gecko/core/services/log.h"
#include "private/labels.h"
#include "vulkan_device.h"
#include "vulkan_util.h"

namespace gecko::graphics {

VulkanCommandList::VulkanCommandList(VulkanDevice& device,
                                     bool compute) noexcept
    : m_Device(&device), m_Compute(compute)
{
  m_Pool = device.AcquireThreadCommandPool();

  VkCommandBufferAllocateInfo allocInfo {};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = m_Pool;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = 1;
  VULKAN_CHECK(
      vkAllocateCommandBuffers(device.Device(), &allocInfo, &m_CmdBuffer));

  // Per-command-list descriptor pool. Reset at Begin(). Sized to handle
  // many BindPipeline calls per frame without running out.
  VkDescriptorPoolSize sizes[] = {
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 128},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 128},
      {VK_DESCRIPTOR_TYPE_SAMPLER, 64},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 128},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 64},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 32},
  };
  VkDescriptorPoolCreateInfo descPoolCreateInfo {};
  descPoolCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descPoolCreateInfo.maxSets = 128;
  descPoolCreateInfo.poolSizeCount = sizeof(sizes) / sizeof(sizes[0]);
  descPoolCreateInfo.pPoolSizes = sizes;
  VULKAN_CHECK(vkCreateDescriptorPool(device.Device(), &descPoolCreateInfo,
                                      nullptr, &m_DescPool));
}

VulkanCommandList::~VulkanCommandList()
{
  if (m_Device == nullptr)
    return;
  // Safe to free directly: VulkanDevice::ReapPending only destroys entries
  // whose fence has signalled (GPU done), and app-side destruction before
  // Execute* means no GPU work was submitted with this command buffer.
  if (m_DescPool != VK_NULL_HANDLE)
    vkDestroyDescriptorPool(m_Device->Device(), m_DescPool, nullptr);
  if (m_CmdBuffer != VK_NULL_HANDLE && m_Pool != VK_NULL_HANDLE)
    vkFreeCommandBuffers(m_Device->Device(), m_Pool, 1, &m_CmdBuffer);
}

void VulkanCommandList::Begin() noexcept
{
  vkResetCommandBuffer(m_CmdBuffer, 0);
  if (m_DescPool != VK_NULL_HANDLE)
    vkResetDescriptorPool(m_Device->Device(), m_DescPool, 0);
  VkCommandBufferBeginInfo beginInfo {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  VULKAN_CHECK(vkBeginCommandBuffer(m_CmdBuffer, &beginInfo));

  m_TouchedCount = 0;
  m_ActiveSwapchain = nullptr;
  m_ActiveSwapchainImageIndex = 0;
  m_ActiveOffscreenRT = nullptr;
  m_CurrentPipeline = nullptr;
  m_CurrentDescSet = VK_NULL_HANDLE;
}

void VulkanCommandList::End() noexcept
{
  // Transition any active swapchain images still in color-attachment layout
  // to PRESENT. The most recent BeginRendering on a swapchain image left it
  // in COLOR_ATTACHMENT_OPTIMAL after EndRendering.
  VULKAN_CHECK(vkEndCommandBuffer(m_CmdBuffer));
}

void VulkanCommandList::TransitionToColorAttachment(
    VulkanSwapchainData* data, u32 imageIndex) noexcept
{
  const VkImageLayout oldLayout = data->ImageLayouts[imageIndex];

  VkImageMemoryBarrier barrier {};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = oldLayout;
  barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = data->Images[imageIndex];
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.layerCount = 1;
  barrier.srcAccessMask =
      (oldLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) ? 0 : 0;
  barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  const VkPipelineStageFlags srcStage =
      (oldLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
          ? VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
          : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

  vkCmdPipelineBarrier(m_CmdBuffer, srcStage,
                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0,
                       nullptr, 0, nullptr, 1, &barrier);

  data->ImageLayouts[imageIndex] = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
}

void VulkanCommandList::TransitionToPresent(VulkanSwapchainData* data,
                                            u32 imageIndex) noexcept
{
  const VkImageLayout oldLayout = data->ImageLayouts[imageIndex];

  VkImageMemoryBarrier barrier {};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = oldLayout;
  barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = data->Images[imageIndex];
  barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.layerCount = 1;
  barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
  barrier.dstAccessMask = 0;
  vkCmdPipelineBarrier(m_CmdBuffer,
                       VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
                       VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);

  data->ImageLayouts[imageIndex] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
}

// Generic two-sided layout transition used for offscreen RTs flipping
// between COLOR_ATTACHMENT and SHADER_READ_ONLY as they're drawn-to and
// sampled within a single command buffer.
void VulkanCommandList::TransitionImage(VkImage image,
                                        VkImageAspectFlags aspect,
                                        VkImageLayout oldLayout,
                                        VkImageLayout newLayout, u32 baseMip,
                                        u32 mipCount, u32 baseLayer,
                                        u32 layerCount) noexcept
{
  if (oldLayout == newLayout)
    return;

  VkImageMemoryBarrier barrier {};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = oldLayout;
  barrier.newLayout = newLayout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image = image;
  barrier.subresourceRange.aspectMask = aspect;
  barrier.subresourceRange.baseMipLevel = baseMip;
  barrier.subresourceRange.levelCount = mipCount;
  barrier.subresourceRange.baseArrayLayer = baseLayer;
  barrier.subresourceRange.layerCount = layerCount;

  VkPipelineStageFlags srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  VkPipelineStageFlags dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

  switch (oldLayout)
  {
  case VK_IMAGE_LAYOUT_UNDEFINED:
    barrier.srcAccessMask = 0;
    srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    break;
  case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
    barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    srcStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    break;
  case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
    barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    srcStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    break;
  case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
    barrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    srcStage = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    break;
  default:
    barrier.srcAccessMask = 0;
    srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    break;
  }

  switch (newLayout)
  {
  case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
    barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    dstStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    break;
  case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    dstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    break;
  case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    dstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    break;
  case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
    barrier.dstAccessMask = 0;
    dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    break;
  default:
    barrier.dstAccessMask = 0;
    dstStage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
    break;
  }

  vkCmdPipelineBarrier(m_CmdBuffer, srcStage, dstStage, 0, 0, nullptr, 0,
                       nullptr, 1, &barrier);
}

void VulkanCommandList::MaybeRecordSwapchain(const RenderTarget& rt) noexcept
{
  if (!rt.Data)
    return;
  auto* rtd = static_cast<VulkanRTData*>(rt.Data.get());
  if (rtd->RTKind != VulkanRTData::Kind::Swapchain ||
      rtd->SwapchainData == nullptr)
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
  m_Touched[m_TouchedCount++] = {rtd->SwapchainData, rtd->FrameIndex,
                                 rtd->ImageIndex};
}

void VulkanCommandList::BeginRendering(const RenderTarget& color,
                                       const ClearValue* clear) noexcept
{
  if (!color.Data)
    return;
  auto* rtd = static_cast<VulkanRTData*>(color.Data.get());

  MaybeRecordSwapchain(color);

  if (rtd->RTKind == VulkanRTData::Kind::Swapchain)
  {
    TransitionToColorAttachment(rtd->SwapchainData, rtd->ImageIndex);
    m_ActiveSwapchain = rtd->SwapchainData;
    m_ActiveSwapchainImageIndex = rtd->ImageIndex;
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

  VkRenderingAttachmentInfo colorAtt {};
  colorAtt.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
  colorAtt.imageView = rtd->ImageView;
  colorAtt.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  colorAtt.loadOp =
      clear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
  colorAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  if (clear != nullptr && clear->Type == ClearValueType::RenderTarget)
  {
    colorAtt.clearValue.color.float32[0] = clear->Color[0];
    colorAtt.clearValue.color.float32[1] = clear->Color[1];
    colorAtt.clearValue.color.float32[2] = clear->Color[2];
    colorAtt.clearValue.color.float32[3] = clear->Color[3];
  }

  VkRenderingInfo renderingInfo {};
  renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  renderingInfo.renderArea.extent = {color.Desc.Width, color.Desc.Height};
  renderingInfo.layerCount = 1;
  renderingInfo.colorAttachmentCount = 1;
  renderingInfo.pColorAttachments = &colorAtt;
  vkCmdBeginRendering(m_CmdBuffer, &renderingInfo);
}

void VulkanCommandList::BeginRendering(
    ::std::span<const RenderTarget* const> colors, const RenderTarget* depth,
    ::std::span<const ClearValue> clears) noexcept
{
  if (colors.empty() && depth == nullptr)
    return;

  VkRenderingAttachmentInfo colorAtts[RenderTargetDesc::MaxRenderTargets] {};
  u32 width = 0;
  u32 height = 0;
  u32 validCount = 0;

  for (u32 i = 0; i < colors.size(); ++i)
  {
    const RenderTarget* c = colors[i];
    if (c == nullptr || !c->Data)
      continue;
    auto* rtd = static_cast<VulkanRTData*>(c->Data.get());

    MaybeRecordSwapchain(*c);
    if (rtd->RTKind == VulkanRTData::Kind::Swapchain)
    {
      TransitionToColorAttachment(rtd->SwapchainData, rtd->ImageIndex);
      m_ActiveSwapchain = rtd->SwapchainData;
      m_ActiveSwapchainImageIndex = rtd->ImageIndex;
    }

    const u32 slot = validCount++;
    colorAtts[slot].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    colorAtts[slot].imageView = rtd->ImageView;
    colorAtts[slot].imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    const bool hasClear = clears.size() > i;
    colorAtts[slot].loadOp =
        hasClear ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAtts[slot].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    if (hasClear)
    {
      colorAtts[slot].clearValue.color.float32[0] = clears[i].Color[0];
      colorAtts[slot].clearValue.color.float32[1] = clears[i].Color[1];
      colorAtts[slot].clearValue.color.float32[2] = clears[i].Color[2];
      colorAtts[slot].clearValue.color.float32[3] = clears[i].Color[3];
    }
    width = c->Desc.Width;
    height = c->Desc.Height;
  }

  VkRenderingInfo renderingInfo {};
  renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  renderingInfo.renderArea.extent = {width, height};
  renderingInfo.layerCount = 1;
  renderingInfo.colorAttachmentCount = validCount;
  renderingInfo.pColorAttachments = colorAtts;
  // Depth handling is deferred with offscreen RT implementation.
  (void)depth;
  vkCmdBeginRendering(m_CmdBuffer, &renderingInfo);
}

void VulkanCommandList::EndRendering() noexcept
{
  vkCmdEndRendering(m_CmdBuffer);

  if (m_ActiveSwapchain != nullptr)
  {
    TransitionToPresent(m_ActiveSwapchain, m_ActiveSwapchainImageIndex);
    m_ActiveSwapchain = nullptr;
    m_ActiveSwapchainImageIndex = 0;
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

void VulkanCommandList::SetViewport(f32 x, f32 y, f32 write, f32 h, f32 minD,
                                    f32 maxD) noexcept
{
  VkViewport viewport {};
  viewport.x = x;
  viewport.y =
      y + h;  // flip Y: Vulkan clip-space is -Y-up; VK_KHR_maintenance1
  viewport.width = write;
  viewport.height = -h;
  viewport.minDepth = minD;
  viewport.maxDepth = maxD;
  vkCmdSetViewport(m_CmdBuffer, 0, 1, &viewport);
}

void VulkanCommandList::SetScissor(i32 x, i32 y, u32 write, u32 h) noexcept
{
  VkRect2D rect {};
  rect.offset = {x, y};
  rect.extent = {write, h};
  vkCmdSetScissor(m_CmdBuffer, 0, 1, &rect);
}

namespace {

// Allocate (or reuse) one descriptor set for the currently bound pipeline.
VkDescriptorSet EnsureCurrentDescSet(VulkanCommandList* self, VkDevice device,
                                     VkDescriptorPool pool,
                                     VulkanPipelineData* pd,
                                     VkDescriptorSet& slot) noexcept
{
  (void)self;
  if (slot != VK_NULL_HANDLE || pd == nullptr ||
      pd->DescSetLayout == VK_NULL_HANDLE)
    return slot;
  VkDescriptorSetAllocateInfo allocInfo {};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = pool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &pd->DescSetLayout;
  if (vkAllocateDescriptorSets(device, &allocInfo, &slot) != VK_SUCCESS)
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
  m_CurrentDescSet = VK_NULL_HANDLE;  // fresh set on next Bind* call
}

void VulkanCommandList::BindPipeline(const ComputePipeline& pipeline) noexcept
{
  if (!pipeline.Data)
    return;
  auto* pd = static_cast<VulkanPipelineData*>(pipeline.Data.get());
  vkCmdBindPipeline(m_CmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pd->Pipeline);
  m_CurrentPipeline = pd;
  m_CurrentDescSet = VK_NULL_HANDLE;
}

void VulkanCommandList::BindVertexBuffer(const Buffer& buffer,
                                         u32 slot) noexcept
{
  if (!buffer.Data)
    return;
  auto* bd = static_cast<VulkanBufferData*>(buffer.Data.get());
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
  if (m_Device == nullptr || m_CurrentPipeline == nullptr ||
      m_CurrentPipeline->DescSetLayout == VK_NULL_HANDLE || !buffer.Data)
    return;
  auto* bd = static_cast<VulkanBufferData*>(buffer.Data.get());
  if (bd->Buffer == VK_NULL_HANDLE)
    return;

  VkDescriptorSet set =
      EnsureCurrentDescSet(this, m_Device->Device(), m_DescPool,
                           m_CurrentPipeline, m_CurrentDescSet);
  if (set == VK_NULL_HANDLE)
    return;

  VkDescriptorBufferInfo bufferInfo {};
  bufferInfo.buffer = bd->Buffer;
  bufferInfo.offset = 0;
  bufferInfo.range = VK_WHOLE_SIZE;

  VkWriteDescriptorSet write {};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = set;
  write.dstBinding = slot;
  write.descriptorCount = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  write.pBufferInfo = &bufferInfo;
  vkUpdateDescriptorSets(m_Device->Device(), 1, &write, 0, nullptr);

  const VkPipelineBindPoint bindPoint = m_CurrentPipeline->IsCompute
                                            ? VK_PIPELINE_BIND_POINT_COMPUTE
                                            : VK_PIPELINE_BIND_POINT_GRAPHICS;
  vkCmdBindDescriptorSets(m_CmdBuffer, bindPoint, m_CurrentPipeline->Layout, 0,
                          1, &set, 0, nullptr);
}

void VulkanCommandList::BindTexture(u32 slot, const Texture& texture) noexcept
{
  if (m_Device == nullptr || m_CurrentPipeline == nullptr ||
      m_CurrentPipeline->DescSetLayout == VK_NULL_HANDLE)
    return;
  if (!texture.Data)
    return;
  auto* td = static_cast<VulkanTextureData*>(texture.Data.get());
  if (td->ImageView == VK_NULL_HANDLE)
    return;

  VkDescriptorSet set =
      EnsureCurrentDescSet(this, m_Device->Device(), m_DescPool,
                           m_CurrentPipeline, m_CurrentDescSet);
  if (set == VK_NULL_HANDLE)
    return;

  VkDescriptorType dtype = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  if (slot < m_CurrentPipeline->NumBindings)
    dtype = m_CurrentPipeline->BindingTypes[slot];

  VkDescriptorImageInfo imageInfo {};
  imageInfo.imageView = td->ImageView;
  imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imageInfo.sampler = VK_NULL_HANDLE;

  VkWriteDescriptorSet write {};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = set;
  write.dstBinding = slot;
  write.descriptorCount = 1;
  write.descriptorType = dtype;
  write.pImageInfo = &imageInfo;
  vkUpdateDescriptorSets(m_Device->Device(), 1, &write, 0, nullptr);

  const VkPipelineBindPoint bindPoint = m_CurrentPipeline->IsCompute
                                            ? VK_PIPELINE_BIND_POINT_COMPUTE
                                            : VK_PIPELINE_BIND_POINT_GRAPHICS;
  vkCmdBindDescriptorSets(m_CmdBuffer, bindPoint, m_CurrentPipeline->Layout, 0,
                          1, &set, 0, nullptr);
}

void VulkanCommandList::BindRWTexture(u32 slot, const Texture& texture) noexcept
{
  if (m_Device == nullptr || m_CurrentPipeline == nullptr ||
      m_CurrentPipeline->DescSetLayout == VK_NULL_HANDLE || !texture.Data)
    return;
  auto* td = static_cast<VulkanTextureData*>(texture.Data.get());
  if (td->ImageView == VK_NULL_HANDLE)
    return;

  VkDescriptorSet set =
      EnsureCurrentDescSet(this, m_Device->Device(), m_DescPool,
                           m_CurrentPipeline, m_CurrentDescSet);
  if (set == VK_NULL_HANDLE)
    return;

  // Storage images must be in GENERAL layout.
  if (td->CurrentLayout != VK_IMAGE_LAYOUT_GENERAL)
  {
    TransitionImage(td->Image, td->Aspect, td->CurrentLayout,
                    VK_IMAGE_LAYOUT_GENERAL);
    td->CurrentLayout = VK_IMAGE_LAYOUT_GENERAL;
  }

  VkDescriptorImageInfo imageInfo {};
  imageInfo.imageView = td->ImageView;
  imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

  VkWriteDescriptorSet write {};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = set;
  write.dstBinding = slot;
  write.descriptorCount = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  write.pImageInfo = &imageInfo;
  vkUpdateDescriptorSets(m_Device->Device(), 1, &write, 0, nullptr);

  const VkPipelineBindPoint bindPoint = m_CurrentPipeline->IsCompute
                                            ? VK_PIPELINE_BIND_POINT_COMPUTE
                                            : VK_PIPELINE_BIND_POINT_GRAPHICS;
  vkCmdBindDescriptorSets(m_CmdBuffer, bindPoint, m_CurrentPipeline->Layout, 0,
                          1, &set, 0, nullptr);
}

void VulkanCommandList::BindSampler(u32 slot, const Sampler& sampler) noexcept
{
  if (m_Device == nullptr || m_CurrentPipeline == nullptr ||
      m_CurrentPipeline->DescSetLayout == VK_NULL_HANDLE || !sampler.Data)
    return;
  auto* sd = static_cast<VulkanSamplerData*>(sampler.Data.get());
  if (sd->Sampler == VK_NULL_HANDLE)
    return;

  VkDescriptorSet set =
      EnsureCurrentDescSet(this, m_Device->Device(), m_DescPool,
                           m_CurrentPipeline, m_CurrentDescSet);
  if (set == VK_NULL_HANDLE)
    return;

  VkDescriptorImageInfo imageInfo {};
  imageInfo.sampler = sd->Sampler;

  VkWriteDescriptorSet write {};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = set;
  write.dstBinding = slot;
  write.descriptorCount = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER;
  write.pImageInfo = &imageInfo;
  vkUpdateDescriptorSets(m_Device->Device(), 1, &write, 0, nullptr);

  const VkPipelineBindPoint bindPoint = m_CurrentPipeline->IsCompute
                                            ? VK_PIPELINE_BIND_POINT_COMPUTE
                                            : VK_PIPELINE_BIND_POINT_GRAPHICS;
  vkCmdBindDescriptorSets(m_CmdBuffer, bindPoint, m_CurrentPipeline->Layout, 0,
                          1, &set, 0, nullptr);
}

namespace {
void BindStorageBuffer(VulkanCommandList* self, VulkanDevice* device,
                       VkCommandBuffer cmd, VkDescriptorPool pool,
                       VulkanPipelineData* pd, VkDescriptorSet& cur, u32 slot,
                       const Buffer& buffer) noexcept
{
  if (device == nullptr || pd == nullptr ||
      pd->DescSetLayout == VK_NULL_HANDLE || !buffer.Data)
    return;
  auto* bd = static_cast<VulkanBufferData*>(buffer.Data.get());
  if (bd->Buffer == VK_NULL_HANDLE)
    return;
  VkDescriptorSet set =
      EnsureCurrentDescSet(self, device->Device(), pool, pd, cur);
  if (set == VK_NULL_HANDLE)
    return;

  VkDescriptorBufferInfo bufferInfo {};
  bufferInfo.buffer = bd->Buffer;
  bufferInfo.offset = 0;
  bufferInfo.range = VK_WHOLE_SIZE;

  VkWriteDescriptorSet write {};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = set;
  write.dstBinding = slot;
  write.descriptorCount = 1;
  write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  write.pBufferInfo = &bufferInfo;
  vkUpdateDescriptorSets(device->Device(), 1, &write, 0, nullptr);

  const VkPipelineBindPoint bindPoint = pd->IsCompute
                                            ? VK_PIPELINE_BIND_POINT_COMPUTE
                                            : VK_PIPELINE_BIND_POINT_GRAPHICS;
  vkCmdBindDescriptorSets(cmd, bindPoint, pd->Layout, 0, 1, &set, 0, nullptr);
}
}  // namespace

void VulkanCommandList::BindStructuredBuffer(u32 slot,
                                             const Buffer& buffer) noexcept
{
  BindStorageBuffer(this, m_Device, m_CmdBuffer, m_DescPool, m_CurrentPipeline,
                    m_CurrentDescSet, slot, buffer);
}

void VulkanCommandList::BindRWStructuredBuffer(u32 slot,
                                               const Buffer& buffer) noexcept
{
  BindStorageBuffer(this, m_Device, m_CmdBuffer, m_DescPool, m_CurrentPipeline,
                    m_CurrentDescSet, slot, buffer);
}

void VulkanCommandList::SetConstants(
    u32 offset, ::std::span<const ::gecko::byte> bytes) noexcept
{
  if (m_CurrentPipeline == nullptr ||
      m_CurrentPipeline->PushConstantBytes == 0 || bytes.empty())
    return;
  const u32 size = static_cast<u32>(bytes.size());
  if (offset + size > m_CurrentPipeline->PushConstantBytes)
    return;
  const VkShaderStageFlags stages = m_CurrentPipeline->IsCompute
                                        ? VK_SHADER_STAGE_COMPUTE_BIT
                                        : VK_SHADER_STAGE_ALL_GRAPHICS;
  vkCmdPushConstants(m_CmdBuffer, m_CurrentPipeline->Layout, stages, offset,
                     size, bytes.data());
}

void VulkanCommandList::Draw(u32 vertexCount, u32 instanceCount,
                             u32 firstVertex, u32 firstInstance) noexcept
{
  vkCmdDraw(m_CmdBuffer, vertexCount, instanceCount, firstVertex,
            firstInstance);
}

void VulkanCommandList::DrawIndexed(u32 indexCount, u32 instanceCount,
                                    u32 firstIndex, i32 vertexOffset,
                                    u32 firstInstance) noexcept
{
  vkCmdDrawIndexed(m_CmdBuffer, indexCount, instanceCount, firstIndex,
                   vertexOffset, firstInstance);
}

void VulkanCommandList::DrawIndirect(const Buffer& buffer, u64 offset,
                                     u32 drawCount, u32 stride) noexcept
{
  if (!buffer.Data)
    return;
  auto* bd = static_cast<VulkanBufferData*>(buffer.Data.get());
  vkCmdDrawIndirect(m_CmdBuffer, bd->Buffer, offset, drawCount, stride);
}

void VulkanCommandList::DrawIndexedIndirect(const Buffer& buffer, u64 offset,
                                            u32 drawCount, u32 stride) noexcept
{
  if (!buffer.Data)
    return;
  auto* bd = static_cast<VulkanBufferData*>(buffer.Data.get());
  vkCmdDrawIndexedIndirect(m_CmdBuffer, bd->Buffer, offset, drawCount, stride);
}

void VulkanCommandList::Dispatch(u32 x, u32 y, u32 z) noexcept
{
  vkCmdDispatch(m_CmdBuffer, x, y, z);
}

void VulkanCommandList::DispatchIndirect(const Buffer& buffer,
                                         u64 offset) noexcept
{
  if (!buffer.Data)
    return;
  auto* bd = static_cast<VulkanBufferData*>(buffer.Data.get());
  vkCmdDispatchIndirect(m_CmdBuffer, bd->Buffer, offset);
}

void VulkanCommandList::TransitionTextureForRead(
    const Texture& texture) noexcept
{
  if (!texture.Data)
    return;
  auto* td = static_cast<VulkanTextureData*>(texture.Data.get());
  if (td->Image == VK_NULL_HANDLE)
    return;
  if (td->CurrentLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    return;
  TransitionImage(td->Image, td->Aspect, td->CurrentLayout,
                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  td->CurrentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void VulkanCommandList::CopyBuffer(const Buffer& dst, u64 dstOffset,
                                   const Buffer& src, u64 srcOffset,
                                   u64 size) noexcept
{
  if (!dst.Data || !src.Data || size == 0)
    return;
  auto* dd = static_cast<VulkanBufferData*>(dst.Data.get());
  auto* sd = static_cast<VulkanBufferData*>(src.Data.get());
  VkBufferCopy region {};
  region.srcOffset = srcOffset;
  region.dstOffset = dstOffset;
  region.size = size;
  vkCmdCopyBuffer(m_CmdBuffer, sd->Buffer, dd->Buffer, 1, &region);
}

void VulkanCommandList::CopyBufferToTexture(const Texture& dst, u32 mip,
                                            u32 slice, const Buffer& src,
                                            u64 srcOffset) noexcept
{
  if (!dst.Data || !src.Data)
    return;
  auto* td = static_cast<VulkanTextureData*>(dst.Data.get());
  auto* bd = static_cast<VulkanBufferData*>(src.Data.get());

  const VkImageLayout prev = td->CurrentLayout;
  TransitionImage(td->Image, td->Aspect, prev,
                  VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  VkBufferImageCopy region {};
  region.bufferOffset = srcOffset;
  region.imageSubresource.aspectMask = td->Aspect;
  region.imageSubresource.mipLevel = mip;
  region.imageSubresource.baseArrayLayer = slice;
  region.imageSubresource.layerCount = 1;
  region.imageExtent = {td->Width, td->Height, 1};
  vkCmdCopyBufferToImage(m_CmdBuffer, bd->Buffer, td->Image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  TransitionImage(td->Image, td->Aspect, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                  VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  td->CurrentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

void VulkanCommandList::CopyTextureToBuffer(const Buffer& dst, u64 dstOffset,
                                            const Texture& src, u32 mip,
                                            u32 slice) noexcept
{
  if (!dst.Data || !src.Data)
    return;
  auto* bd = static_cast<VulkanBufferData*>(dst.Data.get());
  auto* td = static_cast<VulkanTextureData*>(src.Data.get());

  const VkImageLayout prev = td->CurrentLayout;
  TransitionImage(td->Image, td->Aspect, prev,
                  VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

  VkBufferImageCopy region {};
  region.bufferOffset = dstOffset;
  region.imageSubresource.aspectMask = td->Aspect;
  region.imageSubresource.mipLevel = mip;
  region.imageSubresource.baseArrayLayer = slice;
  region.imageSubresource.layerCount = 1;
  region.imageExtent = {td->Width, td->Height, 1};
  vkCmdCopyImageToBuffer(m_CmdBuffer, td->Image,
                         VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, bd->Buffer, 1,
                         &region);

  TransitionImage(td->Image, td->Aspect, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                  prev);
  td->CurrentLayout = prev;
}

void VulkanCommandList::ResetTimestamps(const QueryPool& pool, u32 first,
                                        u32 count) noexcept
{
  if (!pool.IsValid())
    return;
  auto* qd = static_cast<VulkanQueryPoolData*>(pool.Data.get());
  vkCmdResetQueryPool(m_CmdBuffer, qd->QueryPool, first, count);
}

void VulkanCommandList::WriteTimestamp(const QueryPool& pool,
                                       u32 index) noexcept
{
  if (!pool.IsValid())
    return;
  auto* qd = static_cast<VulkanQueryPoolData*>(pool.Data.get());
  vkCmdWriteTimestamp(m_CmdBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                      qd->QueryPool, index);
}

}  // namespace gecko::graphics
#endif
