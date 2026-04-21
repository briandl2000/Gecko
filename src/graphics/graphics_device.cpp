#include "gecko/graphics/graphics_device.h"

#include "gecko/graphics/device.h"
#include "private/null_device.h"

namespace gecko::graphics {

GraphicsDevice::GraphicsDevice()
    : m_Device(CreateUnique<NullDevice>())
{
  m_Device->Init();
}

GraphicsDevice::~GraphicsDevice()
{
  m_Device->Shutdown();
}

// ── Swapchain management ──────────────────────────────────────────────────

Swapchain GraphicsDevice::CreateSwapchain(
    const ::gecko::platform::NativeWindowHandle& native,
    const ::gecko::platform::WindowDesc&         windowDesc,
    const SwapchainDesc&                         desc) noexcept
{
  return m_Device->CreateSwapchain(native, windowDesc, desc);
}

void GraphicsDevice::DestroySwapchain(Swapchain& swapchain) noexcept
{
  m_Device->DestroySwapchain(swapchain);
}

void GraphicsDevice::ResizeSwapchain(Swapchain& swapchain, u32 width,
                                     u32 height) noexcept
{
  m_Device->ResizeSwapchain(swapchain, width, height);
}

RenderTarget GraphicsDevice::GetCurrentBackBuffer(
    const Swapchain& swapchain) const noexcept
{
  return m_Device->GetCurrentBackBuffer(swapchain);
}

u32 GraphicsDevice::GetCurrentBackBufferIndex(
    const Swapchain& swapchain) const noexcept
{
  return m_Device->GetCurrentBackBufferIndex(swapchain);
}

void GraphicsDevice::Present(const Swapchain& swapchain) noexcept
{
  m_Device->Present(swapchain);
}

// ── Command lists ─────────────────────────────────────────────────────────

Unique<ICommandList> GraphicsDevice::CreateGraphicsCommandList() noexcept
{
  return m_Device->CreateGraphicsCommandList();
}

void GraphicsDevice::ExecuteGraphicsCommandList(
    Unique<ICommandList> commandList) noexcept
{
  m_Device->ExecuteGraphicsCommandList(::std::move(commandList));
}

Unique<ICommandList> GraphicsDevice::CreateComputeCommandList() noexcept
{
  return m_Device->CreateComputeCommandList();
}

void GraphicsDevice::ExecuteComputeCommandList(
    Unique<ICommandList> commandList) noexcept
{
  m_Device->ExecuteComputeCommandList(::std::move(commandList));
}

// ── Resource creation ─────────────────────────────────────────────────────

RenderTarget GraphicsDevice::CreateRenderTarget(
    const RenderTargetDesc& desc) noexcept
{
  return m_Device->CreateRenderTarget(desc);
}

Buffer GraphicsDevice::CreateVertexBuffer(
    const VertexBufferDesc& desc) noexcept
{
  return m_Device->CreateVertexBuffer(desc);
}

Buffer GraphicsDevice::CreateIndexBuffer(
    const IndexBufferDesc& desc) noexcept
{
  return m_Device->CreateIndexBuffer(desc);
}

Buffer GraphicsDevice::CreateConstantBuffer(
    const ConstantBufferDesc& desc) noexcept
{
  return m_Device->CreateConstantBuffer(desc);
}

Buffer GraphicsDevice::CreateStructuredBuffer(
    const StructuredBufferDesc& desc) noexcept
{
  return m_Device->CreateStructuredBuffer(desc);
}

Texture GraphicsDevice::CreateTexture(const TextureDesc& desc) noexcept
{
  return m_Device->CreateTexture(desc);
}

GraphicsPipeline GraphicsDevice::CreateGraphicsPipeline(
    const GraphicsPipelineDesc& desc) noexcept
{
  return m_Device->CreateGraphicsPipeline(desc);
}

ComputePipeline GraphicsDevice::CreateComputePipeline(
    const ComputePipelineDesc& desc) noexcept
{
  return m_Device->CreateComputePipeline(desc);
}

// ── Data upload ───────────────────────────────────────────────────────────

void GraphicsDevice::UploadTextureData(
    Texture& texture, ::std::span<const ::gecko::byte> data, u32 mip,
    u32 slice) noexcept
{
  m_Device->UploadTextureData(texture, data, mip, slice);
}

void GraphicsDevice::UploadBufferData(Buffer& buffer,
                                      ::std::span<const ::gecko::byte> data,
                                      u32 offset) noexcept
{
  m_Device->UploadBufferData(buffer, data, offset);
}

}  // namespace gecko::graphics
