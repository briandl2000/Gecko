#ifdef DIRECTX_12
#pragma once

#include "Rendering/DX12/CommonHeaders_DX12.h"

#include "Objects_DX12.h"
#include "Rendering/Backend/CommandList.h"
#include "Rendering/Backend/Device.h"


namespace Gecko { namespace DX12 { 
	
	enum class PipelineType
	{
		None = 0,
		Graphics,
		Compute,
		Raytracing
	};

	class CommandList_DX12 : public CommandList
	{
	public:
		CommandList_DX12() = default;

		virtual ~CommandList_DX12() override;

		virtual bool IsValid() override;

		virtual void ClearRenderTarget(RenderTarget renderTarget) override;

		virtual void CopyTextureToTexture(Texture src, Texture dst) override;

		virtual void BindRenderTarget(RenderTarget renderTarget) override;
		virtual void BindVertexBuffer(VertexBuffer vertexBuffer) override;
		virtual void BindIndexBuffer(IndexBuffer indexBuffer) override;
		virtual void BindTexture(u32 slot, Texture texture) override;
		virtual void BindTexture(u32 slot, Texture texture, u32 mipLevel) override;
		virtual void BindAsRWTexture(u32 slot, Texture texture) override;
		virtual void BindAsRWTexture(u32 slot, Texture texture, u32 mipLevel) override;
	
		virtual void BindGraphicsPipeline(GraphicsPipeline Pipeline) override;
		virtual void BindComputePipeline(ComputePipeline Pipeline) override;

		virtual void BindConstantBuffer(u32 slot, ConstantBuffer buffer) override;

		virtual void SetLocalData(u32 size, void* data) override;

		virtual void Draw(u32 numIndices) override;
		virtual void DrawAuto(u32 numVertices) override;

		virtual void Dispatch(u32 xThreads, u32 yThreads, u32 zThreads) override;

	public:
		void TransitionSubResource(Ref<Resource> resource, D3D12_RESOURCE_STATES transtion, u32 numMips, 
			u32 numArraySlices, u32 subResourceIndex);
		void TransitionResource(Ref<Resource> resource, D3D12_RESOURCE_STATES transtion, u32 numMips,
			u32 numArraySlices);
		void TransitionRenderTarget(RenderTarget renderTarget, D3D12_RESOURCE_STATES newRenderTargetState, 
			D3D12_RESOURCE_STATES newDepthStencilState);
		
		Ref<CommandBuffer> CommandBuffer;

	private:
		void GetTextureMipTransitionBarriers(std::vector<CD3DX12_RESOURCE_BARRIER>* barriers, Ref<Resource> resource, 
			D3D12_RESOURCE_STATES transtion, u32 numMips, u32 numArraySlices, u32 subResourceIndex);
		void GetTextureTransitionBarriers(std::vector<CD3DX12_RESOURCE_BARRIER>* barriers, Ref<Resource> resource, 
			D3D12_RESOURCE_STATES transtion, u32 numMips, u32 numArraySlices);

		PipelineType m_BoundPipelineType;
		GraphicsPipeline m_GraphicsPipeline;
		ComputePipeline m_ComputePipeline;
	};


} }

#endif