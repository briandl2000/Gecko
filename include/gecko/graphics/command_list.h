#pragma once

#include "gecko/core/api.h"
#include "gecko/graphics/graphics_types.h"

#include <span>

namespace gecko::graphics {

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
/// including against back buffers from different swapchains — the backend
/// tracks touched swapchains and coalesces their wait/signal semaphores on
/// submit.
class ICommandList
{
public:
  virtual ~ICommandList() = default;

  ICommandList(const ICommandList&)            = delete("ICommandList is not copyable");
  ICommandList& operator=(const ICommandList&) = delete("ICommandList is not copyable");

  ICommandList(ICommandList&&) noexcept            = default;
  ICommandList& operator=(ICommandList&&) noexcept = default;

  GECKO_API virtual void Begin() noexcept = 0;
  GECKO_API virtual void End() noexcept   = 0;

  [[nodiscard]]
  GECKO_API virtual bool IsValid() const noexcept = 0;

  GECKO_API virtual void BeginRendering(
      const RenderTarget& color,
      const ClearValue*   clear = nullptr) noexcept = 0;

  GECKO_API virtual void BeginRendering(
      ::std::span<const RenderTarget* const> colors,
      const RenderTarget*                    depth,
      ::std::span<const ClearValue>          clears) noexcept = 0;

  GECKO_API virtual void EndRendering() noexcept = 0;

  GECKO_API virtual void SetViewport(f32 x, f32 y, f32 width, f32 height,
                                      f32 minDepth = 0.0F,
                                      f32 maxDepth = 1.0F) noexcept = 0;

  GECKO_API virtual void SetScissor(i32 x, i32 y, u32 width,
                                     u32 height) noexcept = 0;

  GECKO_API virtual void BindPipeline(
      const GraphicsPipeline& pipeline) noexcept = 0;

  GECKO_API virtual void BindPipeline(
      const ComputePipeline& pipeline) noexcept = 0;

  GECKO_API virtual void BindVertexBuffer(const Buffer& buffer,
                                           u32 slot = 0) noexcept = 0;

  GECKO_API virtual void BindIndexBuffer(const Buffer& buffer) noexcept = 0;

  GECKO_API virtual void BindConstantBuffer(u32 slot,
                                             const Buffer& buffer) noexcept = 0;

  GECKO_API virtual void BindTexture(u32 slot,
                                      const Texture& texture) noexcept = 0;

  GECKO_API virtual void Draw(u32 vertexCount, u32 instanceCount = 1,
                               u32 firstVertex   = 0,
                               u32 firstInstance = 0) noexcept = 0;

  GECKO_API virtual void DrawIndexed(u32 indexCount, u32 instanceCount = 1,
                                      u32 firstIndex    = 0,
                                      i32 vertexOffset  = 0,
                                      u32 firstInstance = 0) noexcept = 0;

  GECKO_API virtual void Dispatch(u32 groupsX, u32 groupsY,
                                   u32 groupsZ) noexcept = 0;

protected:
  ICommandList() = default;
};

}  // namespace gecko::graphics
