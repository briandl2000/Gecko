#pragma once
#ifdef WIN32

#include "CommonHeaders_DX12.h"

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

		virtual void ClearRenderTarget(Ref<RenderTarget> renderTarget) override;

		virtual void CopyToRenderTarget(Ref<RenderTarget> src, RenderTargetType srcType, Ref<RenderTarget> dst, RenderTargetType dstType) override;
		virtual void CopyFromRenderTarget(Ref<RenderTarget> src, RenderTargetType srcType, Ref<Texture> dst) override;
		virtual void CopyFromTexture(Ref<Texture> src, Ref<RenderTarget> dst, RenderTargetType dstType) override;

		virtual void BindRenderTarget(Ref<RenderTarget> renderTarget) override;
		virtual void BindVertexBuffer(Ref<VertexBuffer> vertexBuffer) override;
		virtual void BindIndexBuffer(Ref<IndexBuffer> indexBuffer) override;
		virtual void BindTexture(u32 slot, Ref<Texture> texture) override;
		virtual void BindTexture(u32 slot, Ref<Texture> texture, u32 mipLevel) override;
		virtual void BindTexture(u32 slot, Ref<RenderTarget> renderTarget, RenderTargetType type) override;
		virtual void BindConstantBuffer(u32 slot, Ref<ConstantBuffer> buffer) override;
		virtual void BindAsRWTexture(u32 slot, Ref<Texture> texture) override;
		virtual void BindAsRWTexture(u32 slot, Ref<Texture> texture, u32 mipLevel) override;
		virtual void BindAsRWTexture(u32 slot, Ref<RenderTarget> renderTarget, RenderTargetType type) override;
	
		virtual void BindGraphicsPipeline(GraphicsPipeline Pipeline) override;
		virtual void BindComputePipeline(ComputePipeline Pipeline) override;
		virtual void BindRaytracingPipeline(RaytracingPipeline Pipeline) override;

		virtual void SetDynamicCallData(u32 size, void* data) override;

		virtual void BindTLAS(TLAS tlas) override;

		virtual void Draw(u32 numIndices) override;
		virtual void DrawAuto(u32 Vertices) override;

		virtual void Dispatch(u32 xThreads, u32 yThreads, u32 zThreads) override;
		virtual void DispatchRays(u32 width, u32 height, u32 depth) override;

		void TransitionResource(
			Ref<Resource> resource,
			D3D12_RESOURCE_STATES transtion,
			u32 numMips,
			u32 numArraySlices
		);
		void TransitionSubResource(
			Ref<Resource> resource, 
			D3D12_RESOURCE_STATES transtion, 
			u32 numMips,
			u32 numArraySlices,
			u32 subResourceIndex = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES
		);
		void TransitionRenderTarget(Ref<RenderTarget> renderTarget, D3D12_RESOURCE_STATES newRenderTargetState, D3D12_RESOURCE_STATES newDepthStencilState);
		
		PipelineType m_BoundPipelineType;

		GraphicsPipeline m_GraphicsPipeline;
		ComputePipeline m_ComputePipeline;
		RaytracingPipeline m_RaytracingPipeline;
		

		Ref<CommandBuffer> CommandBuffer;

	private:
	};


} }

#endif