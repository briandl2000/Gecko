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

		virtual void ClearRenderTarget(RenderTarget renderTarget) = 0;

		virtual void CopyTextureToTexture(Texture src, Texture dst) = 0;

		virtual void BindRenderTarget(RenderTarget renderTarget) = 0;
		virtual void BindVertexBuffer(VertexBuffer vertexBuffer) = 0;
		virtual void BindIndexBuffer(IndexBuffer indexBuffer) = 0;
		virtual void BindTexture(u32 slot, Texture texture) = 0;
		virtual void BindTexture(u32 slot, Texture texture, u32 mipLevel) = 0;
		virtual void BindAsRWTexture(u32 slot, Texture texture) = 0;
		virtual void BindAsRWTexture(u32 slot, Texture texture, u32 mipLevel) = 0;

		virtual void BindGraphicsPipeline(GraphicsPipeline Pipeline) = 0;
		virtual void BindComputePipeline(ComputePipeline Pipeline) = 0;

		virtual void BindConstantBuffer(u32 slot, ConstantBuffer Buffer) = 0;

		virtual void SetDynamicCallData(u32 size, void* data) = 0;

		virtual void Draw(u32 numIndices) = 0;
		virtual void DrawAuto(u32 Vertices) = 0;

		virtual void Dispatch(u32 xThreads, u32 yThreads, u32 zThreads) = 0;

	private:
	};

}