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

			Device_DX12();
			virtual ~Device_DX12() override;

			virtual void Init() override {}
			virtual void Shutdown() override;

			virtual Ref<CommandList> CreateGraphicsCommandList() override;
			virtual void ExecuteGraphicsCommandList(Ref<CommandList> commandList) override;
			virtual void ExecuteGraphicsCommandListAndFlip(Ref<CommandList> commandList) override;

			virtual Ref<CommandList> CreateComputeCommandList() override;
			virtual void ExecuteComputeCommandList(Ref<CommandList> commandList) override;

			virtual RenderTarget GetCurrentBackBuffer() override;

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

			virtual u32 GetNumBackBuffers() { return m_NumBackBuffers; };
			virtual u32 GetCurrentBackBufferIndex() { return m_CurrentBackBufferIndex; };

		public:
			const ComPtr<ID3D12Device8> GetDevice() { return m_Device; };

			void SetDeferredReleasesFlag();

			void Flush();

			DescriptorHeap& GetRtvHeap() { return m_RtvDescHeap; }
			DescriptorHeap& GetDsvHeap() { return m_DsvDescHeap; }
			DescriptorHeap& GetSrvHeap() { return m_SrvDescHeap; }
			DescriptorHeap& GetUavHeap() { return m_UavDescHeap; }

			Ref<CommandBuffer> GetFreeGraphicsCommandBuffer();
			Ref<CommandBuffer> GetFreeComputeCommandBuffer();
			Ref<CommandBuffer> GetFreeCopyCommandBuffer();

			void ExecuteGraphicsCommandBuffer(Ref<CommandBuffer> copyCommandBuffer);
			void ExecuteComputeCommandBuffer(Ref<CommandBuffer> copyCommandBuffer);
			void ExecuteCopyCommandBuffer(Ref<CommandBuffer> copyCommandBuffer);

			bool Resize(const Event::EventData& data);

		private:
			Texture CreateTexture(const TextureDesc& desc, DXGI_FORMAT format, D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE, D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE, D3D12_CLEAR_VALUE* clearValue = nullptr);

			void CopyToResource(ComPtr<ID3D12Resource>& resource, D3D12_SUBRESOURCE_DATA& subResourceData, u32 subResource = 0);

			void CreateDevice();
			void CreateBackBuffer(u32 width, u32 height);

			void ProcessDeferredReleases();

			ComPtr<IDXGIAdapter4> GetAdapter(ComPtr<IDXGIFactory6> factory);

			D3D_FEATURE_LEVEL GetMaxFeatureLevel(IDXGIAdapter1* adapter);

			// Device Data
			ComPtr<ID3D12Device8> m_Device;
			ComPtr<IDXGIFactory7> m_Factory;

			// Heaps
			DescriptorHeap m_RtvDescHeap{ D3D12_DESCRIPTOR_HEAP_TYPE_RTV };
			DescriptorHeap m_DsvDescHeap{ D3D12_DESCRIPTOR_HEAP_TYPE_DSV };
			DescriptorHeap m_SrvDescHeap{ D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV };
			DescriptorHeap m_UavDescHeap{ D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV };

			DescriptorHandle imGuiHandle;

			std::mutex m_DeferredReleasesMutex;
			// Back buffers

			std::vector<RenderTarget> m_BackBuffers;

			ComPtr<IDXGISwapChain4> m_SwapChain;

			u32 m_NumBackBuffers;

			static constexpr u32 c_NumGraphicsCommandBuffers = 1;
			static constexpr u32 c_NumComputeCommandBuffers = 1;
			static constexpr u32 c_NumCopyCommandBuffers = 1;

			std::array<Ref<CommandBuffer>, c_NumGraphicsCommandBuffers> m_GraphicsCommandBuffers;
			std::array<Ref<CommandBuffer>, c_NumComputeCommandBuffers> m_ComputeCommandBuffers;
			std::array<Ref<CommandBuffer>, c_NumCopyCommandBuffers> m_CopyCommandBuffers;

			HANDLE m_FenceEvent{ nullptr };

			ComPtr<ID3D12CommandQueue> m_GraphicsCommandQueue{ nullptr };
			ComPtr<ID3D12CommandQueue> m_ComputeCommandQueue{ nullptr };
			ComPtr<ID3D12CommandQueue> m_CopyCommandQueue{ nullptr };

			u32 m_CurrentBackBufferIndex;

			Format m_BackBufferFormat;
		};

	}
}
#endif