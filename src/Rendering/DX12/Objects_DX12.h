#ifdef DIRECTX_12
#pragma once
#include "Rendering/DX12/CommonHeaders_DX12.h"

#include "Rendering/Backend/Objects.h"
#include "Rendering/DX12/DescriptorHeap_DX12.h"

namespace Gecko { namespace DX12 {

	class Device_DX12;
	struct DescriptorHandle;

	DXGI_FORMAT FormatToD3D12Format(DataFormat format);
	D3D12_SHADER_VISIBILITY ShaderVisibilityToD3D12ShaderVisibility(ShaderVisibility visibility);
	D3D12_CULL_MODE CullModeToD3D12CullMode(CullMode cullMode);
	D3D12_FILL_MODE PrimitiveTypeToD3D12FillMode(PrimitiveType type);
	D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTypeToD3D12PrimitiveTopologyType(PrimitiveType type);
	D3D_PRIMITIVE_TOPOLOGY PrimitiveTypeToD3D12PrimitiveTopology(PrimitiveType type);
	D3D12_SRV_DIMENSION TextureTypeToD3D12SrvDimension(TextureType textureType);
	D3D12_UAV_DIMENSION TextureTypeToD3D12UavDimension(TextureType textureType);
	D3D12_FILTER SamplerFilterToD3D12Filter(SamplerFilter samplerFilter);
	D3D12_TEXTURE_ADDRESS_MODE AddressModeToD3D12AddressMode(SamplerWrapMode wrapMode);

	struct Resource
	{
		ComPtr<ID3D12Resource> ResourceDX12{ nullptr };
		D3D12_RESOURCE_STATES CurrentState{ D3D12_RESOURCE_STATE_COMMON };
		std::vector<D3D12_RESOURCE_STATES> SubResourceStates{ };
	};

	struct CommandBuffer
	{
		ComPtr<ID3D12CommandAllocator> CommandAllocator{ nullptr };
		u64 FenceValue{ 0 };
		ComPtr<ID3D12Fence1> Fence{ nullptr };
		u32 DeferredReleasesFlag{ 0 };
		ComPtr<ID3D12GraphicsCommandList6> CommandList{ nullptr };

		void Wait(HANDLE fenceEvent)
		{
			ASSERT(Fence && fenceEvent);

			if (IsBusy())
			{
				DIRECTX12_ASSERT(Fence->SetEventOnCompletion(FenceValue, fenceEvent));
				WaitForSingleObject(fenceEvent, INFINITE);
			}
		}

		bool IsBusy()
		{
			bool t = Fence->GetCompletedValue() < FenceValue;
			return t;
		}

		~CommandBuffer();
	};

	struct RenderTarget_DX12
	{
		D3D12_RECT Rect{};
		D3D12_VIEWPORT ViewPort{};
		DescriptorHandle RenderTargetViews[8]{
			DescriptorHandle(), DescriptorHandle(), DescriptorHandle(), DescriptorHandle(),
			DescriptorHandle(), DescriptorHandle(), DescriptorHandle(), DescriptorHandle()
		};
		DescriptorHandle DepthStencilView{};

		RenderTarget_DX12() {}
		~RenderTarget_DX12();
	};

	struct GraphicsPipeline_DX12
	{
		ComPtr<ID3D12RootSignature> RootSignature{ nullptr };
		ComPtr<ID3D12PipelineState> PipelineState{ nullptr };
		std::vector<u32> TextureIndices{ };
		std::vector<u32> ConstantBufferIndices{ };

		u32 LocalDataLocation{ 0 };

		GraphicsPipeline_DX12() {}
		~GraphicsPipeline_DX12();
	};

	struct ComputePipeline_DX12
	{
		ComPtr<ID3D12RootSignature> RootSignature{ nullptr };
		ComPtr<ID3D12PipelineState> PipelineState{ nullptr };
		std::vector<u32> TextureIndices{};
		std::vector<u32> ConstantBufferIndices{};
		std::vector<u32> UAVIndices{};

		u32 LocalDataLocation{ 0 };

		ComputePipeline_DX12() {}
		~ComputePipeline_DX12();
	};

	struct VertexBuffer_DX12
	{
		Ref<Resource> VertexBufferResource{ nullptr };
		D3D12_VERTEX_BUFFER_VIEW VertexBufferView{ };

		VertexBuffer_DX12() {}
		~VertexBuffer_DX12();
	};

	struct IndexBuffer_DX12
	{
		Ref<Resource> IndexBufferResource{ nullptr };
		D3D12_INDEX_BUFFER_VIEW IndexBufferView{ };

		IndexBuffer_DX12() {}
		~IndexBuffer_DX12();
	};

	struct ConstantBuffer_DX12
	{
		Ref<Resource> ConstantBufferResource{ nullptr };
		DescriptorHandle ConstantBufferView{ };
		u64 MemorySize{ 0 };
		void* GPUAddress{ nullptr };

		ConstantBuffer_DX12() {}
		~ConstantBuffer_DX12();
	};
	
	struct Texture_DX12
	{
		Ref<Resource> TextureResource{ nullptr };
		DescriptorHandle ShaderResourceView{ };
		DescriptorHandle UnorderedAccessView{ };
		std::vector<DescriptorHandle> MipShaderResourceViews{ };
		std::vector<DescriptorHandle> MipUnorderedAccessViews{ };

		Texture_DX12() {}
		~Texture_DX12();
	};

} }

#endif