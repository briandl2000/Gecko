#include "null_device.h"

namespace gecko::graphics {

// ── NullCommandList ───────────────────────────────────────────────────────

bool NullCommandList::IsValid() const noexcept { return true; }

void NullCommandList::Begin() noexcept {}
void NullCommandList::End() noexcept {}
void NullCommandList::BeginRendering(const RenderTarget&, bool, bool) noexcept {}
void NullCommandList::EndRendering() noexcept {}
void NullCommandList::SetViewport(f32, f32, f32, f32, f32, f32) noexcept {}
void NullCommandList::SetScissor(u32, u32, u32, u32) noexcept {}
void NullCommandList::BindPipeline(const GraphicsPipeline&) noexcept {}
void NullCommandList::DrawVertices(u32, u32, u32, u32) noexcept {}
void NullCommandList::DrawIndexedVertices(u32, u32, u32, i32, u32) noexcept {}

void NullCommandList::ClearRenderTarget(const RenderTarget&) noexcept {}
void NullCommandList::BindRenderTarget(const RenderTarget&) noexcept {}
void NullCommandList::CopyTextureToTexture(const Texture&,
                                            const Texture&) noexcept {}
void NullCommandList::BindTexture(u32, const Texture&) noexcept {}
void NullCommandList::BindTexture(u32, const Texture&, u32) noexcept {}
void NullCommandList::BindAsRWTexture(u32, const Texture&) noexcept {}
void NullCommandList::BindAsRWTexture(u32, const Texture&, u32) noexcept {}
void NullCommandList::BindVertexBuffer(const Buffer&) noexcept {}
void NullCommandList::BindIndexBuffer(const Buffer&) noexcept {}
void NullCommandList::BindConstantBuffer(u32, const Buffer&) noexcept {}
void NullCommandList::BindStructuredBuffer(u32, const Buffer&) noexcept {}
void NullCommandList::BindAsRWBuffer(u32, const Buffer&) noexcept {}
void NullCommandList::SetLocalData(u32, const void*) noexcept {}
void NullCommandList::BindGraphicsPipeline(const GraphicsPipeline&) noexcept {}
void NullCommandList::BindComputePipeline(const ComputePipeline&) noexcept {}
void NullCommandList::Draw(u32) noexcept {}
void NullCommandList::DrawAuto(u32) noexcept {}
void NullCommandList::Dispatch(u32, u32, u32) noexcept {}

// ── NullDevice ────────────────────────────────────────────────────────────

Swapchain NullDevice::CreateSwapchain(
    const ::gecko::platform::NativeWindowHandle&,
    const SwapchainDesc&) noexcept
{
  return Swapchain{};
}

void NullDevice::DestroySwapchain(Swapchain& swapchain) noexcept
{
  swapchain = Swapchain{};
}

void NullDevice::ResizeSwapchain(Swapchain&) noexcept {}

RenderTarget NullDevice::GetCurrentBackBuffer(const Swapchain&) const noexcept
{
  return RenderTarget{};
}

u32 NullDevice::GetCurrentBackBufferIndex(const Swapchain&) const noexcept
{
  return 0;
}

void NullDevice::Present(const Swapchain&) noexcept {}

Unique<ICommandList> NullDevice::CreateGraphicsCommandList() noexcept
{
  return ::gecko::CreateUnique<NullCommandList>();
}

void NullDevice::ExecuteGraphicsCommandList(Unique<ICommandList>) noexcept {}

Unique<ICommandList> NullDevice::CreateComputeCommandList() noexcept
{
  return ::gecko::CreateUnique<NullCommandList>();
}

void NullDevice::ExecuteComputeCommandList(Unique<ICommandList>) noexcept {}

RenderTarget NullDevice::CreateRenderTarget(const RenderTargetDesc&) noexcept
{
  return RenderTarget{};
}

Buffer NullDevice::CreateVertexBuffer(const VertexBufferDesc&) noexcept
{
  return Buffer{};
}

Buffer NullDevice::CreateIndexBuffer(const IndexBufferDesc&) noexcept
{
  return Buffer{};
}

Buffer NullDevice::CreateConstantBuffer(const ConstantBufferDesc&) noexcept
{
  return Buffer{};
}

Buffer NullDevice::CreateStructuredBuffer(const StructuredBufferDesc&) noexcept
{
  return Buffer{};
}

Texture NullDevice::CreateTexture(const TextureDesc&) noexcept
{
  return Texture{};
}

GraphicsPipeline NullDevice::CreateGraphicsPipeline(
    const GraphicsPipelineDesc&) noexcept
{
  return GraphicsPipeline{};
}

ComputePipeline NullDevice::CreateComputePipeline(
    const ComputePipelineDesc&) noexcept
{
  return ComputePipeline{};
}

void NullDevice::UploadTextureData(Texture&, ::std::span<const ::gecko::byte>,
                                    u32, u32) noexcept
{}

void NullDevice::UploadBufferData(Buffer&, ::std::span<const ::gecko::byte>,
                                   u32) noexcept
{}

}  // namespace gecko::graphics
