#pragma once
#include "Defines.h"

#include "Rendering/Backend/Objects.h"

namespace Gecko
{

	/**
	 * @brief
	 * Enum for selecting what rendering api to use for the backend.
	 */
	enum class RenderAPI
	{
		None = 0,
#ifdef DIRECTX_12
		DX12,
#endif // DIRECTX_12
	};

	static constexpr RenderAPI s_RenderAPI =
#ifdef DIRECTX_12
		RenderAPI::DX12;
#endif // DIRECTX_12

	class CommandList;

	/**
	 * @brief
	 * The Device class is an interface to Create different objects for gpu rendering.
	 * It should have an API specific implementation for each possible RenderAPI.
	 */
	class Device
	{
	public:
		virtual ~Device() {};

		/**
		 * @brief Create a Device object
		 * @return Ref<Device> which is a smart pointer of an API specific implementation of Device.
		 */
		static Ref<Device> CreateDevice();

		/**
		 * @brief
		 * Initialize the Device.
		 * [IMORTANT] This needs to be called after the device gets created.
		 */
		virtual void Init() = 0;
		/**
		 * @brief
		 * Shuts down the Device.
		 * [IMORTANT] This needs to be called before the device runs out of scope.
		 */
		virtual void Shutdown() = 0;

		// TODO: add timing systems for the command lists.

		/**
		 * @brief Create a Graphics Command List object
		 * @return Ref<CommandList> that you can use to record commands
		 */
		virtual Ref<CommandList> CreateGraphicsCommandList() = 0;
		/**
		 * @brief Executes the specified command list
		 * @param commandList the CommandList to be executed (Needs to be created form CreateGraphicsCommandList())
		 */
		virtual void ExecuteGraphicsCommandList(Ref<CommandList> commandList) = 0;
		/**
		 * @brief Executes the specified command list and flips the back buffers.
		 * @param commandList the CommandList to be executed (Needs to be created form CreateGraphicsCommandList())
		 */
		virtual void ExecuteGraphicsCommandListAndFlip(Ref<CommandList> commandList) = 0;

		/**
		 * @brief Create a Compute Command List object
		 * @return Ref<CommandList> that you can use to record commands
		 */
		virtual Ref<CommandList> CreateComputeCommandList() = 0;
		/**
		 * @brief Executes the specified command list
		 * @param commandList the CommandList to be executed (Needs to be created form CreateComputeCommandList())
		 */
		virtual void ExecuteComputeCommandList(Ref<CommandList> commandList) = 0;

		/**
		 * @brief Get the Current Back Buffer object.
		 * [IMPORTANT] You cannot use the textures of the back buffer.
		 * @return RenderTarget the render target of the currrent back buffer
		 */
		virtual RenderTarget GetCurrentBackBuffer() = 0;

		/**
		 * @brief Create a Render Target object
		 * @param desc The specified descriptor of the render target
		 * @return A valid RenderTarget object
		 */
		virtual RenderTarget CreateRenderTarget(const RenderTargetDesc& desc) = 0;

		/**
		 * @brief Create a Vertex Buffer object
		 * @param desc The specified descriptor of the vertex buffer
		 * @return a valid Buffer object with Vertex set as the Type
		 */
		virtual Buffer CreateVertexBuffer(const VertexBufferDesc& desc) = 0;
		/**
		 * @brief Create a Index Buffer object
		 * @param desc The specified descriptor of the index buffer
		 * @return a valid Buffer object with Index set as the Type
		 */
		virtual Buffer CreateIndexBuffer(const IndexBufferDesc& desc) = 0;
		/**
		 * @brief Create a Constant Buffer object
		 * @param desc The specified descriptor of the constant buffer
		 * @return a valid Buffer object with Constant set as the Type
		 */
		virtual Buffer CreateConstantBuffer(const ConstantBufferDesc& desc) = 0;
		/**
		 * @brief Create a Structured Buffer object
		 * @param desc The specified descriptor of the structured buffer
		 * @return a valid Buffer object with Structured set as the Type
		 */
		virtual Buffer CreateStructuredBuffer(const StructuredBufferDesc& desc) = 0;

		/**
		 * @brief Create a Texture object
		 * @param desc The specified descriptor of the texture
		 * @return a valid Texture object
		 */
		virtual Texture CreateTexture(const TextureDesc& desc) = 0;

		/**
		 * @brief Create a Graphics Pipeline object
		 * @param desc The specified descriptor of the graphics pipeline
		 * @return a valid GraphicsPipeline object
		 */
		virtual GraphicsPipeline CreateGraphicsPipeline(const GraphicsPipelineDesc& desc) = 0;
		/**
		 * @brief Create a Compute Pipeline object
		 * @param desc The specified descriptor of the compute pipeline
		 * @return a valid ComputePipeline object
		 */
		virtual ComputePipeline CreateComputePipeline(const ComputePipelineDesc& desc) = 0;

		/**
		 * @brief Uploads data to a Texture object.
		 * [IMPORTANT] make sure the data is in the correct format and size based on the texture descriptor.
		 * @param texture The Texture you want to upload data to
		 * @param data A void* to the data you want to uploads
		 * @param mip What mip to upload to
		 * @param slice What array slice to upload to
		 */
		virtual void UploadTextureData(Texture texture, void* data, u32 mip = 0, u32 slice = 0) = 0;
		/**
		 * @brief Uploads data to a buffer.
		 * @param buffer The Buffer you want to upload data towards
		 * @param data A void* to the data you want to upload
		 * @param size The size of the data in bytes
		 * @param offset The offset of where you want the data to end up in bytes
		 */
		virtual void UploadBufferData(Buffer buffer, void* data, u32 size, u32 offset = 0) = 0;

		virtual u32 GetNumBackBuffers() = 0;
		virtual u32 GetCurrentBackBufferIndex() = 0;
	};

} // namespace Gecko