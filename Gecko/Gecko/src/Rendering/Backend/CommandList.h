#pragma once
#include "Defines.h"

#include "Rendering/Backend/Objects.h"

namespace Gecko {

	struct RenderTarget;

	class CommandList
	{
	public:
		virtual ~CommandList() {};

		virtual bool IsValid() = 0;

		virtual void ClearRenderTarget(Ref<RenderTarget> renderTarget) = 0;

		virtual void CopyToRenderTarget(Ref<RenderTarget> src, RenderTargetType srcType, Ref<RenderTarget> dst, RenderTargetType dstType) = 0;
		virtual void CopyFromRenderTarget(Ref<RenderTarget> src, RenderTargetType srcType, Ref<Texture> dst) = 0;
		virtual void CopyFromTexture(Ref<Texture> src, Ref<RenderTarget> dst, RenderTargetType dstType) = 0;

		virtual void BindRenderTarget(Ref<RenderTarget> renderTarget) = 0;
		virtual void BindVertexBuffer(Ref<VertexBuffer> vertexBuffer) = 0;
		virtual void BindIndexBuffer(Ref<IndexBuffer> indexBuffer) = 0;
		virtual void BindTexture(u32 slot, Ref<Texture> texture) = 0;
		virtual void BindTexture(u32 slot, Ref<Texture> texture, u32 mipLevel) = 0;
		virtual void BindTexture(u32 slot, Ref<RenderTarget> renderTarget, RenderTargetType type) = 0;
		virtual void BindAsRWTexture(u32 slot, Ref<Texture> texture) = 0;
		virtual void BindAsRWTexture(u32 slot, Ref<Texture> texture, u32 mipLevel) = 0;
		virtual void BindAsRWTexture(u32 slot, Ref<RenderTarget> renderTarget, RenderTargetType type) = 0;

		virtual void BindGraphicsPipeline(GraphicsPipeline Pipeline) = 0;
		virtual void BindComputePipeline(ComputePipeline Pipeline) = 0;
		virtual void BindRaytracingPipeline(RaytracingPipeline Pipeline) = 0;

		virtual void BindConstantBuffer(u32 slot, Ref<ConstantBuffer> Buffer) = 0;

		virtual void BindTLAS(TLAS tlas) = 0;

		virtual void SetDynamicCallData(u32 size, void* data) = 0;

		virtual void Draw(u32 numIndices) = 0;
		virtual void DrawAuto(u32 Vertices) = 0;

		virtual void Dispatch(u32 xThreads, u32 yThreads, u32 zThreads) = 0;

		virtual void DispatchRays(u32 width, u32 height, u32 depth) = 0;


	private:
	};

}