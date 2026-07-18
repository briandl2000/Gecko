#include "null_device.h"

namespace gecko::graphics {

Swapchain NullDevice::CreateSwapchain(const gecko::platform::NativeWindowHandle&, const SwapchainDesc&) noexcept
{
  return Swapchain {};
}

void NullDevice::DestroySwapchain(Swapchain& swapchain) noexcept
{
  swapchain = Swapchain {};
}

void NullDevice::ResizeSwapchain(Swapchain&) noexcept
{}

FrameContext NullDevice::BeginFrame(Swapchain&) noexcept
{
  return FrameContext {};
}

void NullDevice::Present(Span<const FrameContext>) noexcept
{}

Unique<ICommandList> NullDevice::CreateGraphicsCommandList() noexcept
{
  return gecko::CreateUnique<NullCommandList>();
}

Unique<ICommandList> NullDevice::CreateComputeCommandList() noexcept
{
  return gecko::CreateUnique<NullCommandList>();
}

void NullDevice::ExecuteGraphicsCommandList(Unique<ICommandList>) noexcept
{}
void NullDevice::ExecuteComputeCommandList(Unique<ICommandList>) noexcept
{}

RenderTarget NullDevice::CreateRenderTarget(const RenderTargetDesc&) noexcept
{
  return RenderTarget {};
}

Buffer NullDevice::CreateVertexBuffer(const VertexBufferDesc&) noexcept
{
  return Buffer {};
}

Buffer NullDevice::CreateIndexBuffer(const IndexBufferDesc&) noexcept
{
  return Buffer {};
}

Buffer NullDevice::CreateConstantBuffer(const ConstantBufferDesc&) noexcept
{
  return Buffer {};
}

Buffer NullDevice::CreateStructuredBuffer(const StructuredBufferDesc&) noexcept
{
  return Buffer {};
}

Texture NullDevice::CreateTexture(const TextureDesc&) noexcept
{
  return Texture {};
}

Sampler NullDevice::CreateSampler(const SamplerDesc&) noexcept
{
  return Sampler {};
}

GraphicsPipeline NullDevice::CreateGraphicsPipeline(const GraphicsPipelineDesc&) noexcept
{
  return GraphicsPipeline {};
}

ComputePipeline NullDevice::CreateComputePipeline(const ComputePipelineDesc&) noexcept
{
  return ComputePipeline {};
}

QueryPool NullDevice::CreateTimestampQueryPool(const QueryPoolDesc&) noexcept
{
  return QueryPool {};
}

u32 NullDevice::ReadTimestamps(const QueryPool&, u32, Span<u64>) noexcept
{
  return 0;
}

void NullDevice::UploadTextureData(Texture&, Span<const gecko::byte>, u32, u32) noexcept
{}

void NullDevice::UploadBufferData(Buffer&, Span<const gecko::byte>, u32) noexcept
{}

}  // namespace gecko::graphics
