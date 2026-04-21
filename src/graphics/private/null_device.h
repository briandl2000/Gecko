#pragma once

#include "gecko/graphics/graphics_device.h"

namespace gecko::graphics {

class NullCommandList final : public ICommandList
{
public:
  NullCommandList()  = default;
  ~NullCommandList() = default;

  NullCommandList(const NullCommandList&)            = delete("NullCommandList is not copyable");
  NullCommandList& operator=(const NullCommandList&) = delete("NullCommandList is not copyable");

  bool IsValid() const noexcept override;

  void ClearRenderTarget(const RenderTarget&) noexcept override;
  void BindRenderTarget(const RenderTarget&) noexcept override;
  void CopyTextureToTexture(const Texture&, const Texture&) noexcept override;
  void BindTexture(u32, const Texture&) noexcept override;
  void BindTexture(u32, const Texture&, u32) noexcept override;
  void BindAsRWTexture(u32, const Texture&) noexcept override;
  void BindAsRWTexture(u32, const Texture&, u32) noexcept override;
  void BindVertexBuffer(const Buffer&) noexcept override;
  void BindIndexBuffer(const Buffer&) noexcept override;
  void BindConstantBuffer(u32, const Buffer&) noexcept override;
  void BindStructuredBuffer(u32, const Buffer&) noexcept override;
  void BindAsRWBuffer(u32, const Buffer&) noexcept override;
  void SetLocalData(u32, const void*) noexcept override;
  void BindGraphicsPipeline(const GraphicsPipeline&) noexcept override;
  void BindComputePipeline(const ComputePipeline&) noexcept override;
  void Draw(u32) noexcept override;
  void DrawAuto(u32) noexcept override;
  void Dispatch(u32, u32, u32) noexcept override;
};

class NullDevice final : public GraphicsDevice
{
public:
  NullDevice()  = default;
  ~NullDevice() = default;

  NullDevice(const NullDevice&)            = delete("NullDevice is not copyable");
  NullDevice& operator=(const NullDevice&) = delete("NullDevice is not copyable");

  Swapchain CreateSwapchain(const ::gecko::platform::NativeWindowHandle&,
                             const SwapchainDesc&) noexcept override;
  void DestroySwapchain(Swapchain& swapchain) noexcept override;
  void ResizeSwapchain(Swapchain&) noexcept override;

  RenderTarget GetCurrentBackBuffer(const Swapchain&) const noexcept override;
  u32          GetCurrentBackBufferIndex(const Swapchain&) const noexcept override;
  void         Present(const Swapchain&) noexcept override;

  Unique<ICommandList> CreateGraphicsCommandList() noexcept override;
  void                 ExecuteGraphicsCommandList(Unique<ICommandList>) noexcept override;
  Unique<ICommandList> CreateComputeCommandList() noexcept override;
  void                 ExecuteComputeCommandList(Unique<ICommandList>) noexcept override;

  RenderTarget    CreateRenderTarget(const RenderTargetDesc&) noexcept override;
  Buffer          CreateVertexBuffer(const VertexBufferDesc&) noexcept override;
  Buffer          CreateIndexBuffer(const IndexBufferDesc&) noexcept override;
  Buffer          CreateConstantBuffer(const ConstantBufferDesc&) noexcept override;
  Buffer          CreateStructuredBuffer(const StructuredBufferDesc&) noexcept override;
  Texture         CreateTexture(const TextureDesc&) noexcept override;
  GraphicsPipeline CreateGraphicsPipeline(const GraphicsPipelineDesc&) noexcept override;
  ComputePipeline  CreateComputePipeline(const ComputePipelineDesc&) noexcept override;

  void UploadTextureData(Texture&, ::std::span<const ::gecko::byte>, u32,
                         u32) noexcept override;
  void UploadBufferData(Buffer&, ::std::span<const ::gecko::byte>,
                        u32) noexcept override;
};

}  // namespace gecko::graphics
