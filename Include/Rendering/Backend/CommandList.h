#pragma once
#include "Defines.h"

#include "Rendering/Backend/Objects.h"

namespace Gecko
{

	struct RenderTarget;

	/**
	 * @brief
	 * The Command class is an interface to record commands for the GPU and later execute them.
	 * It should have an API specific implementation for each possible RenderAPI.
	 */
	class CommandList
	{
	public:
		virtual ~CommandList() {};

		/**
		 * @brief
		 * @return true when CommandList is valid.
		 */
		virtual bool IsValid() = 0;

		/**
		 * @brief Clears the specified render target with the clear value specified in the RenderTargetDesc
		 * @param renderTarget The RenderTarget to be cleared.
		 */
		virtual void ClearRenderTarget(const RenderTarget& renderTarget) = 0;

		/**
		 * @brief 
		 * Copies the content of one texture to another.
		 * [IMPORTANT] both src and dst need to have the same format, width, height and depth
		 * @param src The source image to copy from
		 * @param dst The destination image you copy to
		 */
		virtual void CopyTextureToTexture(const Texture& src, const Texture& dst) = 0;

		// TODO: add a CopyBufferToBuffer method.

		/**
		 * @brief Binds a render target to a graphics pipeline.
		 * @param renderTarget The RenderTarget to be bound
		 */
		virtual void BindRenderTarget(const RenderTarget& renderTarget) = 0;
		
		/**
		 * @brief 
		 * Binds a vertex buffer to a graphics pipeline
		 * @param vertexBuffer The Buffer to be bound needs to be of type Vertex
		 */
		virtual void BindVertexBuffer(const Buffer& vertexBuffer) = 0;
		/**
		 * @brief 
		 * Binds an index buffer to a graphics pipeline
		 * @param indexBuffer The Buffer to be bound needs to be of type Index
		 */
		virtual void BindIndexBuffer(const Buffer& indexBuffer) = 0;
		/**
		 * @brief 
		 * Binds a constant buffer to a pipeline
		 * @param slot The buffer register slot to bind to this reflects the "bX" in HLSL
		 * @param Buffer The buffer to be bound needs to be of type Constant
		 */
		virtual void BindConstantBuffer(u32 slot, const Buffer& buffer) = 0;
		/**
		 * @brief 
		 * Binds a buffer to a pipeline
		 * @param slot The texture register slot to bind to this reflects the "tX" in HLSL
		 * @param Buffer The buffer to be bound needs to be of type Structured, Vertex or Index
		 */
		virtual void BindStructuredBuffer(u32 slot, const Buffer& buffer) = 0;
		/**
		 * @brief 
		 * Binds a buffer to a compute pipeline to read and write to
		 * @param slot The read write register slot to bind to this reflects the "uX" in HLSL
		 * @param Buffer The buffer to be bound needs to be of type Structured, Vertex or Index
		 */
		virtual void BindAsRWBuffer(u32 slot, const Buffer& buffer) = 0;
		/**
		 * @brief Set the Local Data object.
		 * Local data is for once per draw or dispatch call.
		 * This can be usefull if you want to have small changing data per call.
		 * @param size The size in bytes of the data to be uploaded
		 * @param data A void* to the data to be uploaded
		 */
		virtual void SetLocalData(u32 size, void* data) = 0;

		/**
		 * @brief 
		 * Binds a texture to a pipeline
		 * @param slot The texture register slot to bind to this reflects the "tX" in HLSL
		 * @param texture the texture to be bound
		 */
		virtual void BindTexture(u32 slot, const Texture& texture) = 0;
		/**
		 * @brief 
		 * Binds a specific texture mip to a pipeline
		 * @param slot The texture register slot to bind to this reflects the "tX" in HLSL
		 * @param texture the texture to be bound
		 * @param mipLevel the mip level of the texture to be bound
		 */
		virtual void BindTexture(u32 slot, const Texture& texture, u32 mipLevel) = 0;
		/**
		 * @brief 
		 * Binds a texture to a compute pipeline to read and write to
		 * @param slot The read write register slot to bind to this reflects the "uX" in HLSL
		 * @param texture the texture to be bound
		 */
		virtual void BindAsRWTexture(u32 slot, const Texture& texture) = 0;
		/**
		 * @brief 
		 * Binds a specific texture mip to a compute pipeline to read and write to
		 * @param slot The texture register slot to bind to this reflects the "tX" in HLSL
		 * @param texture the texture to be bound
		 * @param mipLevel the mip level of the texture to be bound
		 */
		virtual void BindAsRWTexture(u32 slot, const Texture& texture, u32 mipLevel) = 0;

		/**
		 * @brief 
		 * Binds a graphics pipeline to the command list
		 * @param graphicsPipeline the GraphicsPipeline to be bound 
		 */
		virtual void BindGraphicsPipeline(const GraphicsPipeline& graphicsPipeline) = 0;
		/**
		 * @brief 
		 * Binds a compute pipeline to the command list
		 * @param computePipeline the ComputePipeline to be bound 
		 */
		virtual void BindComputePipeline(const ComputePipeline& computePipeline) = 0;

		/**
		 * @brief 
		 * Runs the graphics pipeline with an Index buffer bound
		 * [IMPORTANT] everything must be bound correctly to the pipeline before running it
		 * @param numIndices the number of indices you want to draw
		 */
		virtual void Draw(u32 numIndices) = 0;
		/**
		 * @brief 
		 * Runs the graphics pipeline without an Index buffer bound
		 * [IMPORTANT] everything must be bound correctly to the pipeline before running it
		 * @param numIndices the number of vertices you want to draw
		 */
		virtual void DrawAuto(u32 numVertices) = 0;
		
		/**
		 * @brief 
		 * Runs the compute pipeline
		 * [IMPORTANT] everything must be bound correctly to the pipeline before running it
		 * @param xThreads The number of x threads you want to run
		 * @param yThreads The number of y threads you want to run
		 * @param zThreads The number of z threads you want to run
		 */
		virtual void Dispatch(u32 xThreads, u32 yThreads, u32 zThreads) = 0;

	private:
	};

}