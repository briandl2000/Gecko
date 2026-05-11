#pragma once

/// @file
/// `ICommandList` interface for recording GPU work.

#include "gecko/core/api.h"
#include "gecko/core/labels.h"
#include "gecko/graphics/graphics_types.h"

#include <span>

namespace gecko::graphics {

class IGpuSampler;

/// Abstract graphics/compute command list.
///
/// Obtained from `GraphicsDevice::CreateGraphicsCommandList()` or
/// `CreateComputeCommandList()`. Expected call order:
///
///     cmd->Begin();
///       cmd->BeginRendering(backBuffer, &clear);
///         cmd->SetViewport(...); cmd->SetScissor(...);
///         cmd->BindPipeline(pipeline);
///         cmd->BindVertexBuffer(vb);
///         cmd->Draw(3);
///       cmd->EndRendering();
///     cmd->End();
///
/// `BeginRendering` may be called multiple times between `Begin`/`End`,
/// including against back buffers from different swapchains -- the backend
/// tracks touched swapchains and coalesces their wait/signal semaphores on
/// submit.
class ICommandList
{
public:
  virtual ~ICommandList() = default;

  ICommandList(const ICommandList&) = delete;
  ICommandList& operator=(const ICommandList&) = delete;

  ICommandList(ICommandList&&) noexcept = default;
  ICommandList& operator=(ICommandList&&) noexcept = default;

  /// Begin recording. Must be paired with `End()`.
  GECKO_API virtual void Begin() noexcept = 0;
  /// Finish recording. After this the list is ready for execution.
  GECKO_API virtual void End() noexcept = 0;

  /// `true` if the command list is in a recordable state.
  [[nodiscard]]
  GECKO_API virtual bool IsValid() const noexcept = 0;

  /// Begin a render pass using a single descriptor object.
  /// @param info  Render pass setup, including attachments and clear/load behavior.
  GECKO_API virtual void BeginRendering(const BeginRenderingInfo& info) noexcept = 0;

  /// End the current render pass started by `BeginRendering`.
  GECKO_API virtual void EndRendering() noexcept = 0;

  /// Set the rasterizer viewport in pixels.
  GECKO_API virtual void SetViewport(f32 x, f32 y, f32 width, f32 height, f32 minDepth = 0.0F,
                                     f32 maxDepth = 1.0F) noexcept = 0;

  /// Set the scissor rectangle in pixels.
  GECKO_API virtual void SetScissor(i32 x, i32 y, u32 width, u32 height) noexcept = 0;

  /// Bind a graphics pipeline for subsequent draws.
  GECKO_API virtual void BindPipeline(const GraphicsPipeline& pipeline) noexcept = 0;

  /// Bind a compute pipeline for subsequent dispatches.
  GECKO_API virtual void BindPipeline(const ComputePipeline& pipeline) noexcept = 0;

  /// Bind a vertex buffer to `slot`.
  GECKO_API virtual void BindVertexBuffer(const Buffer& buffer, u32 slot = 0) noexcept = 0;

  /// Bind a 32-bit-index buffer for `DrawIndexed` / `DrawIndexedIndirect`.
  GECKO_API virtual void BindIndexBuffer(const Buffer& buffer) noexcept = 0;

  /// Bind a constant (uniform) buffer to `slot`.
  GECKO_API virtual void BindConstantBuffer(u32 slot, const Buffer& buffer) noexcept = 0;

  /// Bind a read-only (SRV) structured buffer.
  GECKO_API virtual void BindStructuredBuffer(u32 slot, const Buffer& buffer) noexcept = 0;

  /// Bind a read-write (UAV) structured buffer.
  GECKO_API virtual void BindRWStructuredBuffer(u32 slot, const Buffer& buffer) noexcept = 0;

  /// Bind a read-only (SRV) texture.
  GECKO_API virtual void BindTexture(u32 slot, const Texture& texture) noexcept = 0;

  /// Bind a read-write (UAV) storage texture.
  GECKO_API virtual void BindRWTexture(u32 slot, const Texture& texture) noexcept = 0;

  /// Bind a sampler object to `slot`.
  GECKO_API virtual void BindSampler(u32 slot, const Sampler& sampler) noexcept = 0;

  /// Upload `bytes` into the currently-bound pipeline's push-constant
  /// block starting at `offset`. `offset + bytes.size_bytes()` must be
  /// <= the pipeline's declared `PushConstantBytes`.
  GECKO_API virtual void SetConstants(u32 offset, ::std::span<const ::gecko::byte> bytes) noexcept = 0;

  /// Issue a non-indexed draw.
  GECKO_API virtual void Draw(u32 vertexCount, u32 instanceCount = 1, u32 firstVertex = 0,
                              u32 firstInstance = 0) noexcept = 0;

  /// Issue an indexed draw using the currently bound index buffer.
  GECKO_API virtual void DrawIndexed(u32 indexCount, u32 instanceCount = 1, u32 firstIndex = 0, i32 vertexOffset = 0,
                                     u32 firstInstance = 0) noexcept = 0;

  /// Indirect draw from a buffer of `VkDrawIndirectCommand`-style records
  /// (vertexCount, instanceCount, firstVertex, firstInstance -- four u32).
  GECKO_API virtual void DrawIndirect(const Buffer& buffer, u64 offset, u32 drawCount = 1,
                                      u32 stride = 16) noexcept = 0;

  /// Indirect indexed draw from `VkDrawIndexedIndirectCommand`-style records
  /// (indexCount, instanceCount, firstIndex, vertexOffset, firstInstance).
  GECKO_API virtual void DrawIndexedIndirect(const Buffer& buffer, u64 offset, u32 drawCount = 1,
                                             u32 stride = 20) noexcept = 0;

  /// Dispatch a compute workload of `groupsX * groupsY * groupsZ` workgroups.
  GECKO_API virtual void Dispatch(u32 groupsX, u32 groupsY, u32 groupsZ) noexcept = 0;

  /// Indirect compute dispatch; reads three `u32` group counts at `offset`.
  GECKO_API virtual void DispatchIndirect(const Buffer& buffer, u64 offset) noexcept = 0;

  /// Transition a texture that was last written as a UAV (e.g. by a compute
  /// Dispatch) into the `SHADER_READ_ONLY` state so it can be sampled by a
  /// subsequent graphics pass via `BindTexture`. Must be called *outside*
  /// any `BeginRendering` / `EndRendering` pair.
  GECKO_API virtual void TransitionTextureForRead(const Texture& texture) noexcept = 0;

  // -- Copy commands ---------------------------------------------

  GECKO_API virtual void CopyBuffer(const Buffer& dst, u64 dstOffset, const Buffer& src, u64 srcOffset,
                                    u64 size) noexcept = 0;

  GECKO_API virtual void CopyBufferToTexture(const Texture& dst, u32 mip, u32 slice, const Buffer& src,
                                             u64 srcOffset) noexcept = 0;

  GECKO_API virtual void CopyTextureToBuffer(const Buffer& dst, u64 dstOffset, const Texture& src, u32 mip,
                                             u32 slice) noexcept = 0;

  // -- Timestamp queries -----------------------------------------

  GECKO_API virtual void ResetTimestamps(const QueryPool& pool, u32 first, u32 count) noexcept = 0;

  GECKO_API virtual void WriteTimestamp(const QueryPool& pool, u32 index) noexcept = 0;

  // -- GPU auto-zones --------------------------------------------
  //
  // Attach an IGpuSampler so every Draw/DrawIndexed/DrawIndirect/
  // DrawIndexedIndirect/Dispatch/DispatchIndirect call on this command
  // list is automatically wrapped in a GPU zone at ProfLevel::Detailed.
  // Pass `nullptr` to disable. Manual BeginZone/EndZone keep working
  // alongside; auto-zones nest inside any user-opened zone.
  GECKO_API virtual void AttachGpuSampler(IGpuSampler* sampler, ::gecko::Label autoZoneLabel) noexcept = 0;

  // Sampler currently attached to this command list (or nullptr). Used
  // by GECKO_GPU_SCOPE_* macros so callers don't have to re-thread the
  // sampler reference everywhere.
  GECKO_API virtual IGpuSampler* GetAttachedGpuSampler() const noexcept = 0;

protected:
  ICommandList() = default;
};

}  // namespace gecko::graphics
