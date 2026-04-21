#pragma once

#include "gecko/core/api.h"
#include "gecko/graphics/objects.h"

namespace gecko::graphics {

class ICommandList
{
public:
  virtual ~ICommandList() = default;

  ICommandList(const ICommandList&) = delete("ICommandList is not copyable");
  ICommandList& operator=(
      const ICommandList&) = delete("ICommandList is not copyable");

  ICommandList(ICommandList&&) noexcept            = default;
  ICommandList& operator=(ICommandList&&) noexcept = default;

  [[nodiscard]]
  GECKO_API virtual bool IsValid() const noexcept = 0;

  // ── Render target ─────────────────────────────────────────────

  GECKO_API virtual void ClearRenderTarget(
      const RenderTarget& renderTarget) noexcept = 0;

  GECKO_API virtual void BindRenderTarget(
      const RenderTarget& renderTarget) noexcept = 0;

  // ── Texture operations ────────────────────────────────────────

  GECKO_API virtual void CopyTextureToTexture(const Texture& src,
                                               const Texture& dst) noexcept = 0;

  GECKO_API virtual void BindTexture(u32 slot,
                                      const Texture& texture) noexcept = 0;

  GECKO_API virtual void BindTexture(u32 slot, const Texture& texture,
                                      u32 mipLevel) noexcept = 0;

  GECKO_API virtual void BindAsRWTexture(u32 slot,
                                          const Texture& texture) noexcept = 0;

  GECKO_API virtual void BindAsRWTexture(u32 slot, const Texture& texture,
                                          u32 mipLevel) noexcept = 0;

  // ── Buffer operations ─────────────────────────────────────────

  GECKO_API virtual void BindVertexBuffer(
      const Buffer& vertexBuffer) noexcept = 0;

  GECKO_API virtual void BindIndexBuffer(
      const Buffer& indexBuffer) noexcept = 0;

  GECKO_API virtual void BindConstantBuffer(u32 slot,
                                             const Buffer& buffer) noexcept = 0;

  GECKO_API virtual void BindStructuredBuffer(
      u32 slot, const Buffer& buffer) noexcept = 0;

  GECKO_API virtual void BindAsRWBuffer(u32 slot,
                                         const Buffer& buffer) noexcept = 0;

  GECKO_API virtual void SetLocalData(u32 sizeInBytes,
                                       const void* data) noexcept = 0;

  // ── Pipeline binding ──────────────────────────────────────────

  GECKO_API virtual void BindGraphicsPipeline(
      const GraphicsPipeline& pipeline) noexcept = 0;

  GECKO_API virtual void BindComputePipeline(
      const ComputePipeline& pipeline) noexcept = 0;

  // ── Draw / dispatch ───────────────────────────────────────────

  GECKO_API virtual void Draw(u32 numIndices) noexcept = 0;

  GECKO_API virtual void DrawAuto(u32 numVertices) noexcept = 0;

  GECKO_API virtual void Dispatch(u32 xThreads, u32 yThreads,
                                   u32 zThreads) noexcept = 0;

protected:
  ICommandList() = default;
};

}  // namespace gecko::graphics
