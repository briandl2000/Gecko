#pragma once
#ifdef WIN32

#include <vector>
#include <array>

#include "CommonHeaders_DX12.h"

#include "Rendering/DX12/Objects_DX12.h"
#include "Rendering/Backend/Device.h"
#include "Rendering/DX12/Resources_DX12.h"

#include "Core/Event.h"

namespace Gecko { namespace DX12
{
		constexpr D3D_FEATURE_LEVEL c_MinimumFeatureLevel{ D3D_FEATURE_LEVEL_11_0 };

		struct RenderTarget_DX12;
		
		class Device_DX12 : protected Event::EventListener<Device_DX12>, public Device
		{
		public:
			
			Device_DX12() = default;
			// Device Interface Methods

			virtual ~Device_DX12() override {};

			virtual void Init() override;
			virtual void Shutdown() override;

			virtual Ref<CommandList> CreateGraphicsCommandList() override;
			virtual void ExecuteGraphicsCommandList(Ref<CommandList> commandList) override;
			virtual void ExecuteGraphicsCommandListAndFlip(Ref<CommandList> commandList) override;

			virtual Ref<CommandList> CreateComputeCommandList() override;
			virtual void ExecuteComputeCommandList(Ref<CommandList> commandList) override;

			virtual RenderTarget CreateRenderTarget(const RenderTargetDesc& desc) override;
			virtual VertexBuffer CreateVertexBuffer(const VertexBufferDesc& desc) override;
			virtual IndexBuffer CreateIndexBuffer(const IndexBufferDesc& desc) override;
			virtual GraphicsPipeline CreateGraphicsPipeline(const GraphicsPipelineDesc& desc) override;
			virtual ComputePipeline CreateComputePipeline(const ComputePipelineDesc& desc) override;
			virtual Texture CreateTexture(const TextureDesc& desc) override;
			virtual ConstantBuffer CreateConstantBuffer(const ConstantBufferDesc& desc) override;

			virtual void UploadTextureData(Texture texture, void* Data, u32 mip = 0, u32 slice = 0) override;

			virtual void DrawTextureInImGui(Texture texture, u32 width = 0, u32 height = 0) override;
			virtual void ImGuiRender(Ref<CommandList> commandList) override;

			virtual RenderTarget GetCurrentBackBuffer() { return m_BackBuffers[m_CurrentBackBufferIndex]; }
			virtual u32 GetNumBackBuffers() { return m_NumBackBuffers; };
			virtual u32 GetCurrentBackBufferIndex() { return m_CurrentBackBufferIndex; };

			static void ClearResource(Ref<Resource> resource);

		public:
			// Public Methods

			void SetDeferredReleasesFlag();
			
			void Flush();

			Ref<CommandBuffer> GetFreeGraphicsCommandBuffer();
			Ref<CommandBuffer> GetFreeComputeCommandBuffer();
			Ref<CommandBuffer> GetFreeCopyCommandBuffer();

			void ExecuteGraphicsCommandBuffer(Ref<CommandBuffer> graphicsCommandBuffer);
			void ExecuteComputeCommandBuffer(Ref<CommandBuffer> computeCommandBuffer);
			void ExecuteCopyCommandBuffer(Ref<CommandBuffer> copyCommandBuffer);

			bool Resize(const Event::EventData& data);

			// TODO: this is currently only used by Resource_DX12 which should be changed
			const ComPtr<ID3D12Device8> GetDevice() { return m_Device; };

			DescriptorHeap& GetRtvHeap() { return m_RtvDescHeap; }
			DescriptorHeap& GetDsvHeap() { return m_DsvDescHeap; }
			DescriptorHeap& GetSrvHeap() { return m_SrvDescHeap; }

			static void FlagResrouceForDeletion(Ref<Resource>& resource);
			static void FlagPipelineStateForDeletion(ComPtr<ID3D12PipelineState>& pipelineState);
			static void FlagRtvDescriptorHandleForDeletion(DescriptorHandle& handle);
			static void FlagDsvDescriptorHandleForDeletion(DescriptorHandle& handle);
			static void FlagSrvDescriptorHandleForDeletion(DescriptorHandle& handle);

		private:
			// Private Methods

			void CreateDevice();

			D3D_FEATURE_LEVEL GetMaxFeatureLevel(const ComPtr<IDXGIAdapter1>& adapter);
			ComPtr<IDXGIAdapter4> GetAdapter(const ComPtr<IDXGIFactory6>& factory);
			template<typename T>
			void CreateCommandBuffers(const ComPtr<ID3D12Device8>& device, ComPtr<ID3D12CommandQueue>* commandQueue, T* commandBuffers, D3D12_COMMAND_LIST_TYPE type);

			Texture CreateTexture(
				const TextureDesc& desc, 
				DXGI_FORMAT format, 
				D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE, 
				D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE, 
				const D3D12_CLEAR_VALUE* clearValue = nullptr);
			
			void CopyToResource(ComPtr<ID3D12Resource>& resource, D3D12_SUBRESOURCE_DATA& subResourceData, u32 subResource = 0);
			void RecreateBackBuffers(u32 width, u32 height);
			void ProcessDeferredReleases();

			// Device Data
			ComPtr<ID3D12Device8> m_Device{ nullptr };
			ComPtr<IDXGIFactory7> m_Factory{ nullptr };

			// Heaps
			DescriptorHeap m_RtvDescHeap{ D3D12_DESCRIPTOR_HEAP_TYPE_RTV };
			DescriptorHeap m_DsvDescHeap{ D3D12_DESCRIPTOR_HEAP_TYPE_DSV };
			DescriptorHeap m_SrvDescHeap{ D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV };

			std::vector<Ref<Resource>> m_ResourcesToBeDeleted{ };
			std::vector<DescriptorHandle> m_RtvDescHeapHandlesToBeDeleted{ };
			std::vector<DescriptorHandle> m_DsvDescHeapHandlesToBeDeleted{ };
			std::vector<DescriptorHandle> m_SrvDescHeapHandlesToBeDeleted{ };
			std::vector<ComPtr<ID3D12PipelineState>> m_PipelineStatesToBeDeleted{ };

			DescriptorHandle m_ImGuiHandle{ };

			std::mutex m_DeferredReleasesMutex{ };

			// Back buffers
			std::vector<RenderTarget> m_BackBuffers{ };

			// Swap chain
			ComPtr<IDXGISwapChain4> m_SwapChain{ nullptr };

			// Command lists
			// TODO: Maybe change them to vectors and create new command buffers when needed?
			static constexpr u32 c_NumGraphicsCommandBuffers = 3;
			static constexpr u32 c_NumComputeCommandBuffers = 3;
			static constexpr u32 c_NumCopyCommandBuffers = 3;
			std::array<Ref<CommandBuffer>, c_NumGraphicsCommandBuffers> m_GraphicsCommandBuffers;
			std::array<Ref<CommandBuffer>, c_NumComputeCommandBuffers> m_ComputeCommandBuffers;
			std::array<Ref<CommandBuffer>, c_NumCopyCommandBuffers> m_CopyCommandBuffers;
			ComPtr<ID3D12CommandQueue> m_GraphicsCommandQueue{ nullptr };
			ComPtr<ID3D12CommandQueue> m_ComputeCommandQueue{ nullptr };
			ComPtr<ID3D12CommandQueue> m_CopyCommandQueue{ nullptr };

			HANDLE m_FenceEvent{ nullptr };

			u32 m_NumBackBuffers{ 0 };
			u32 m_CurrentBackBufferIndex{ 0 };

			DataFormat m_BackBufferFormat{ DataFormat::None };
		};

	}
}
#endif