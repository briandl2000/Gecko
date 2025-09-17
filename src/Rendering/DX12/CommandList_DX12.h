#ifdef DIRECTX_12
#pragma once

#include "Rendering/DX12/CommonHeaders_DX12.h"

#include "Objects_DX12.h"
#include "Rendering/Backend/CommandList.h"
#include "Rendering/Backend/Device.h"


namespace Gecko::DX12
{

  enum class PipelineType
  {
    None = 0,
    Graphics,
    Compute,
    Raytracing
  };

  // Helper function
  std::string EnumToString(PipelineType val);

  class CommandList_DX12 : public CommandList
  {
  public:
    CommandList_DX12() = default;

    virtual ~CommandList_DX12() override;

    virtual bool IsValid() override;

    virtual void ClearRenderTarget(const RenderTarget& renderTarget) override;

    virtual void CopyTextureToTexture(const Texture& src, const Texture& dst) override;

    virtual void BindRenderTarget(const RenderTarget& renderTarget) override;

    virtual void BindVertexBuffer(const Buffer& vertexBuffer) override;
    virtual void BindIndexBuffer(const Buffer& indexBuffer) override;
    virtual void BindConstantBuffer(u32 slot, const Buffer& buffer) override;
    virtual void BindStructuredBuffer(u32 slot, const Buffer& buffer) override;
    virtual void BindAsRWBuffer(u32 slot, const Buffer& buffer) override;
    virtual void SetLocalData(u32 size, void* data) override;

    virtual void BindTexture(u32 slot, const Texture& texture) override;
    virtual void BindTexture(u32 slot, const Texture& texture, u32 mipLevel) override;
    virtual void BindAsRWTexture(u32 slot, const Texture& texture) override;
    virtual void BindAsRWTexture(u32 slot, const Texture& texture, u32 mipLevel) override;

    virtual void BindGraphicsPipeline(const GraphicsPipeline& graphicsPipeline) override;
    virtual void BindComputePipeline(const ComputePipeline& computePipeline) override;

    virtual void Draw(u32 numIndices) override;
    virtual void DrawAuto(u32 numVertices) override;
    virtual void Dispatch(u32 xThreads, u32 yThreads, u32 zThreads) override;

  public:
    void TransitionSubResource(const Ref<Resource>& resource, D3D12_RESOURCE_STATES transtion, u32 numMips,
      u32 numArraySlices, u32 subResourceIndex);
    void TransitionResource(const Ref<Resource>& resource, D3D12_RESOURCE_STATES transtion, u32 numMips,
      u32 numArraySlices);
    void TransitionRenderTarget(const RenderTarget& renderTarget, D3D12_RESOURCE_STATES newRenderTargetState,
      D3D12_RESOURCE_STATES newDepthStencilState);

    Ref<CommandBuffer> CommandBuffer;

  private:
    void GetTextureMipTransitionBarriers(std::vector<CD3DX12_RESOURCE_BARRIER>* barriers, const Ref<Resource>& resource,
      D3D12_RESOURCE_STATES transtion, u32 numMips, u32 numArraySlices, u32 subResourceIndex);
    void GetTextureTransitionBarriers(std::vector<CD3DX12_RESOURCE_BARRIER>* barriers, const Ref<Resource>& resource,
      D3D12_RESOURCE_STATES transtion, u32 numMips, u32 numArraySlices);

    PipelineType m_BoundPipelineType;
    GraphicsPipeline m_GraphicsPipeline;
    ComputePipeline m_ComputePipeline;
  };


}

#endif
