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
		virtual void BindVertexBuffer(Buffer vertexBuffer) = 0;
		virtual void BindIndexBuffer(Buffer indexBuffer) = 0;
		virtual void BindTexture(u32 slot, Texture texture) = 0;
		virtual void BindTexture(u32 slot, Texture texture, u32 mipLevel) = 0;
		virtual void BindAsRWTexture(u32 slot, Texture texture) = 0;
		virtual void BindAsRWTexture(u32 slot, Texture texture, u32 mipLevel) = 0;

		virtual void BindGraphicsPipeline(GraphicsPipeline Pipeline) = 0;
		virtual void BindComputePipeline(ComputePipeline Pipeline) = 0;

		virtual void BindConstantBuffer(u32 slot, Buffer Buffer) = 0;
		virtual void BindStructuredBuffer(u32 slot, Buffer Buffer) = 0;

		virtual void BindAsRWBuffer(u32 slot, Buffer buffer) = 0;

		virtual void SetLocalData(u32 size, void* data) = 0;

		virtual void Draw(u64 numIndices) = 0;
		virtual void DrawAuto(u64 numVertices) = 0;

		virtual void Dispatch(u32 xThreads, u32 yThreads, u32 zThreads) = 0;

	private:
	};

}