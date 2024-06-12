#pragma once
#include "Defines.h"

#include "Rendering/Backend/Objects.h"

namespace Gecko {
	
	enum class RenderAPI
	{
		None = 0,
		DX12
	};

	static constexpr RenderAPI s_RenderAPI =
#ifdef WIN32
		RenderAPI::DX12;
#endif

	class CommandList;

	class Device
	{
	public:

		virtual ~Device() {};

		static Ref<Device> CreateDevice();

		// TODO: add timing systems for the command lists.
		// TODO: add a seperate back buffer flip pass.
		virtual Ref<CommandList> CreateGraphicsCommandList() = 0;
		virtual void ExecuteGraphicsCommandList(Ref<CommandList> commandList) = 0;
		virtual void ExecuteGraphicsCommandListAndFlip(Ref<CommandList> commandList) = 0;
		
		virtual Ref<CommandList> CreateComputeCommandList() = 0;
		virtual void ExecuteComputeCommandList(Ref<CommandList> commandList) = 0;

		virtual Ref<RenderTarget> GetCurrentBackBuffer() = 0;

		virtual Ref<RenderTarget> CreateRenderTarget(const RenderTargetDesc& desc) = 0;
		virtual Ref<VertexBuffer> CreateVertexBuffer(const VertexBufferDesc& desc) = 0;
		virtual Ref<IndexBuffer> CreateIndexBuffer(const IndexBufferDesc& desc) = 0;
		virtual GraphicsPipeline CreateGraphicsPipeline(const GraphicsPipelineDesc& desc) = 0;
		virtual ComputePipeline CreateComputePipeline(const ComputePipelineDesc& desc) = 0;
		virtual Ref<Texture> CreateTexture(const TextureDesc& desc) = 0;
		virtual Ref<ConstantBuffer> CreateConstantBuffer(const ConstantBufferDesc& desc) = 0;

		// Raytracing stuff
		virtual RaytracingPipeline CreateRaytracingPipeline(const RaytracingPipelineDesc& desc) = 0;
		virtual BLAS CreateBLAS(const BLASDesc& desc) = 0;
		virtual TLAS CreateTLAS(const TLASRefitDesc& desc) = 0;

		virtual void UploadTextureData(Ref<Texture> texture, void* Data, u32 mip = 0, u32 slice = 0) = 0;

		virtual void DrawTextureInImGui(Ref<Texture> texture, u32 width = 0, u32 height = 0) = 0;
		virtual void DrawRenderTargetInImGui(Ref<RenderTarget> renderTarget, u32 width = 0, u32 height = 0, RenderTargetType type = RenderTargetType::Target0) = 0;
		virtual void ImGuiRender(Ref<CommandList> commandList) = 0;

		virtual bool Destroy() = 0;

	private:

	};

}