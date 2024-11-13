#pragma once
#include "Defines.h"

#include "Rendering/Backend/Objects.h"

namespace Gecko {
	
	enum class RenderAPI
	{
		None = 0,
#ifdef WIN32
		DX12,
#endif
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

		virtual void Init() = 0;
		virtual void Shutdown() = 0;

		// TODO: add timing systems for the command lists.
		// TODO: add a seperate back buffer flip pass.
		virtual Ref<CommandList> CreateGraphicsCommandList() = 0;
		virtual void ExecuteGraphicsCommandList(Ref<CommandList> commandList) = 0;
		virtual void ExecuteGraphicsCommandListAndFlip(Ref<CommandList> commandList) = 0;
		
		virtual Ref<CommandList> CreateComputeCommandList() = 0;
		virtual void ExecuteComputeCommandList(Ref<CommandList> commandList) = 0;

		virtual RenderTarget GetCurrentBackBuffer() = 0;

		virtual RenderTarget CreateRenderTarget(const RenderTargetDesc& desc) = 0;
		virtual VertexBuffer CreateVertexBuffer(const VertexBufferDesc& desc) = 0;
		virtual IndexBuffer CreateIndexBuffer(const IndexBufferDesc& desc) = 0;
		virtual GraphicsPipeline CreateGraphicsPipeline(const GraphicsPipelineDesc& desc) = 0;
		virtual ComputePipeline CreateComputePipeline(const ComputePipelineDesc& desc) = 0;
		virtual Texture CreateTexture(const TextureDesc& desc) = 0;
		virtual Buffer CreateConstantBuffer(const ConstantBufferDesc& desc) = 0;
		//virtual ConstantBuffer CreateStructuredBuffer(const StructuredBufferDesc& desc) = 0;

		virtual void UploadTextureData(Texture texture, void* data, u32 mip = 0, u32 slice = 0) = 0;
		virtual void UploadBufferData(Buffer buffer, void* data, u32 size, u32 offset = 0) = 0;

		// TODO: imgui methods should probably go in the command list
		virtual void DrawTextureInImGui(Texture texture, u32 width = 0, u32 height = 0) = 0;
		virtual void ImGuiRender(Ref<CommandList> commandList) = 0;

		virtual u32 GetNumBackBuffers() = 0;
		virtual u32 GetCurrentBackBufferIndex() = 0;

	private:

	};

}