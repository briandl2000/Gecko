#pragma once

#include "gecko/graphics/graphics_device.h"

namespace gecko::graphics {

class NullCommandList final : public ICommandList
{
public:
  NullCommandList() = default;
  ~NullCommandList() override = default;

  NullCommandList(const NullCommandList&) = delete;
  NullCommandList& operator=(const NullCommandList&) = delete;

  void Begin() noexcept override
  {}
  void End() noexcept override
  {}
  [[nodiscard]] bool IsValid() const noexcept override
  {
    return true;
  }

  void BeginRendering(const RenderTarget&, const ClearValue*) noexcept override
  {}
  void BeginRendering(::std::span<const RenderTarget* const>,
                      const RenderTarget*,
                      ::std::span<const ClearValue>) noexcept override
  {}
  void EndRendering() noexcept override
  {}

  void SetViewport(f32, f32, f32, f32, f32, f32) noexcept override
  {}
  void SetScissor(i32, i32, u32, u32) noexcept override
  {}

  void BindPipeline(const GraphicsPipeline&) noexcept override
  {}
  void BindPipeline(const ComputePipeline&) noexcept override
  {}

  void BindVertexBuffer(const Buffer&, u32) noexcept override
  {}
  void BindIndexBuffer(const Buffer&) noexcept override
  {}
  void BindConstantBuffer(u32, const Buffer&) noexcept override
  {}
  void BindStructuredBuffer(u32, const Buffer&) noexcept override
  {}
  void BindRWStructuredBuffer(u32, const Buffer&) noexcept override
  {}
  void BindTexture(u32, const Texture&) noexcept override
  {}
  void BindRWTexture(u32, const Texture&) noexcept override
  {}
  void BindSampler(u32, const Sampler&) noexcept override
  {}
  void SetConstants(u32, ::std::span<const ::gecko::byte>) noexcept override
  {}

  void Draw(u32, u32, u32, u32) noexcept override
  {}
  void DrawIndexed(u32, u32, u32, i32, u32) noexcept override
  {}
  void DrawIndirect(const Buffer&, u64, u32, u32) noexcept override
  {}
  void DrawIndexedIndirect(const Buffer&, u64, u32, u32) noexcept override
  {}
  void Dispatch(u32, u32, u32) noexcept override
  {}
  void DispatchIndirect(const Buffer&, u64) noexcept override
  {}

  void TransitionTextureForRead(const Texture&) noexcept override
  {}

  void CopyBuffer(const Buffer&, u64, const Buffer&, u64, u64) noexcept override
  {}
  void CopyBufferToTexture(const Texture&, u32, u32, const Buffer&,
                           u64) noexcept override
  {}
  void CopyTextureToBuffer(const Buffer&, u64, const Texture&, u32,
                           u32) noexcept override
  {}

  void ResetTimestamps(const QueryPool&, u32, u32) noexcept override
  {}
  void WriteTimestamp(const QueryPool&, u32) noexcept override
  {}
  void AttachGpuSampler(IGpuSampler*, ::gecko::Label) noexcept override
  {}
  IGpuSampler* GetAttachedGpuSampler() const noexcept override
  {
    return nullptr;
  }
};

class NullDevice final : public GraphicsDevice
{
public:
  NullDevice() = default;
  ~NullDevice() override = default;

  NullDevice(const NullDevice&) = delete;
  NullDevice& operator=(const NullDevice&) = delete;

  Swapchain CreateSwapchain(const ::gecko::platform::NativeWindowHandle&,
                            const SwapchainDesc&) noexcept override;
  void DestroySwapchain(Swapchain& swapchain) noexcept override;
  void ResizeSwapchain(Swapchain&) noexcept override;

  FrameContext BeginFrame(Swapchain&) noexcept override;
  void Present(::std::span<const FrameContext>) noexcept override;

  Unique<ICommandList> CreateGraphicsCommandList() noexcept override;
  Unique<ICommandList> CreateComputeCommandList() noexcept override;
  void ExecuteGraphicsCommandList(Unique<ICommandList>) noexcept override;
  void ExecuteComputeCommandList(Unique<ICommandList>) noexcept override;

  RenderTarget CreateRenderTarget(const RenderTargetDesc&) noexcept override;
  Buffer CreateVertexBuffer(const VertexBufferDesc&) noexcept override;
  Buffer CreateIndexBuffer(const IndexBufferDesc&) noexcept override;
  Buffer CreateConstantBuffer(const ConstantBufferDesc&) noexcept override;
  Buffer CreateStructuredBuffer(const StructuredBufferDesc&) noexcept override;
  Texture CreateTexture(const TextureDesc&) noexcept override;
  Sampler CreateSampler(const SamplerDesc&) noexcept override;
  GraphicsPipeline CreateGraphicsPipeline(
      const GraphicsPipelineDesc&) noexcept override;
  ComputePipeline CreateComputePipeline(
      const ComputePipelineDesc&) noexcept override;
  QueryPool CreateTimestampQueryPool(const QueryPoolDesc&) noexcept override;
  u32 ReadTimestamps(const QueryPool&, u32, ::std::span<u64>) noexcept override;

  ::gecko::Unique<IGpuSampler> CreateGpuSampler(
      const GpuSamplerDesc&) noexcept override
  {
    return nullptr;
  }

  void UploadTextureData(Texture&, ::std::span<const ::gecko::byte>, u32,
                         u32) noexcept override;
  void UploadBufferData(Buffer&, ::std::span<const ::gecko::byte>,
                        u32) noexcept override;
};

}  // namespace gecko::graphics
