#ifdef WIN32
#pragma once

#include "Rendering/DX12/CommonHeaders_DX12.h"
#include "Resources_DX12.h"

#include "Core/Logger.h"
#include "Rendering/Backend/Objects.h"

namespace Gecko { namespace DX12 {

	class Device_DX12;

	static DXGI_FORMAT FormatToD3D12Format(Format format)
	{
		switch (format)
		{
		case Format::R8G8B8A8_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		case Format::R8G8B8A8_UNORM: return DXGI_FORMAT_R8G8B8A8_UNORM;
		
		case Format::R32_FLOAT: return DXGI_FORMAT_R32_FLOAT;
		case Format::R32G32_FLOAT: return DXGI_FORMAT_R32G32_FLOAT;
		case Format::R32G32B32_FLOAT: return DXGI_FORMAT_R32G32B32_FLOAT;
		case Format::R32G32B32A32_FLOAT: return DXGI_FORMAT_R32G32B32A32_FLOAT;
		case Format::R16G16B16A16_FLOAT: return DXGI_FORMAT_R16G16B16A16_FLOAT;


		case Format::R8_UINT: return DXGI_FORMAT_R8_UINT;
		case Format::R16_UINT: return DXGI_FORMAT_R16_UINT;
		case Format::R32_UINT: return DXGI_FORMAT_R32_UINT;

		case Format::R8_INT: return DXGI_FORMAT_R8_SINT;
		case Format::R16_INT: return DXGI_FORMAT_R16_SINT;
		case Format::R32_INT: return DXGI_FORMAT_R32_SINT;

		case Format::None: break;
		}

		ASSERT_MSG(false, "Unkown Format state.");
		return DXGI_FORMAT_UNKNOWN;
	}

	static D3D12_SHADER_VISIBILITY ShaderVisibilityToD3D12ShaderVisibility(ShaderVisibility visibility)
	{
		switch (visibility)
		{
		case ShaderVisibility::All: return D3D12_SHADER_VISIBILITY_ALL;
		case ShaderVisibility::Vertex: return D3D12_SHADER_VISIBILITY_VERTEX;
		case ShaderVisibility::Pixel: return D3D12_SHADER_VISIBILITY_PIXEL;
		}
		ASSERT_MSG(false, "Unkown Shader visibilty");
		return D3D12_SHADER_VISIBILITY_ALL;
	}

	static D3D12_CULL_MODE CullModeToDirectX12CullMode(CullMode cullMode)
	{
		switch (cullMode)
		{
		case CullMode::None: return D3D12_CULL_MODE_NONE;
		case CullMode::Back: return D3D12_CULL_MODE_BACK;
		case CullMode::Front: return D3D12_CULL_MODE_FRONT;
		default: break;
		}

		ASSERT_MSG(false, "Unkown Cullmode");
		return D3D12_CULL_MODE_NONE;
	}

	static D3D12_FILL_MODE GetD3D12FillModeFromPrimitiveType(PrimitiveType type)
	{
		switch (type)
		{
		case PrimitiveType::Lines: return D3D12_FILL_MODE_WIREFRAME;
		case PrimitiveType::Triangles: return D3D12_FILL_MODE_SOLID;
		default: break;
		}
		
		ASSERT_MSG(false, "Unkown primitive type");
		return D3D12_FILL_MODE_WIREFRAME;
	}

	static D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTypeToD3D12PrimitiveTopologyType(PrimitiveType type)
	{
		switch (type)
		{
		case PrimitiveType::Lines: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
		case PrimitiveType::Triangles: return D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		default: break;
		}

		ASSERT_MSG(false, "Unkown primitive type");
		return D3D12_PRIMITIVE_TOPOLOGY_TYPE_UNDEFINED;
	}

	static D3D_PRIMITIVE_TOPOLOGY PrimitiveTypeToD3D12PrimitiveTopology(PrimitiveType type)
	{
		switch (type)
		{
		case PrimitiveType::Lines: return D3D_PRIMITIVE_TOPOLOGY_LINELIST;
		case PrimitiveType::Triangles: return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		default: break;
		}

		ASSERT_MSG(false, "Unkown primitive type");
		return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
	}

	static D3D12_SRV_DIMENSION TextureTypeToD3D12SrvDimension(TextureType textureType)
	{
		switch (textureType)
		{
		case TextureType::Tex1D: return D3D12_SRV_DIMENSION_TEXTURE1D;
		case TextureType::Tex2D: return D3D12_SRV_DIMENSION_TEXTURE2D;
		case TextureType::Tex3D: return D3D12_SRV_DIMENSION_TEXTURE3D;
		case TextureType::TexCube: return D3D12_SRV_DIMENSION_TEXTURECUBE;
		case TextureType::Tex1DArray: return D3D12_SRV_DIMENSION_TEXTURE1DARRAY;
		case TextureType::Tex2DArray: return D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
		}
		ASSERT_MSG(false, "Unkown TextureType");
		return D3D12_SRV_DIMENSION_UNKNOWN;
	}

	static D3D12_UAV_DIMENSION TextureTypeToD3D12UavDimension(TextureType textureType)
	{
		switch (textureType)
		{
		case TextureType::Tex1D: return D3D12_UAV_DIMENSION_TEXTURE1D;
		case TextureType::Tex2D: return D3D12_UAV_DIMENSION_TEXTURE2D;
		case TextureType::Tex3D: return D3D12_UAV_DIMENSION_TEXTURE3D;
		case TextureType::TexCube: return D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		case TextureType::Tex1DArray: return D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
		case TextureType::Tex2DArray: return D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
		}
		ASSERT_MSG(false, "Unkown TextureType");
		return D3D12_UAV_DIMENSION_UNKNOWN;
	}

	static D3D12_FILTER SamplerFilterToD3D12Filter(SamplerFilter samplerFilter)
	{
		switch (samplerFilter)
		{
		case SamplerFilter::Linear: return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		case SamplerFilter::Point: return D3D12_FILTER_MIN_MAG_MIP_POINT;
		}

		ASSERT_MSG(false, "Unkown samplerFilter");
		return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	}

	static D3D12_TEXTURE_ADDRESS_MODE AddressModeToD3D12AddressMode(SamplerWrapMode wrapMode)
	{
		switch (wrapMode)
		{
		case SamplerWrapMode::Wrap: return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		case SamplerWrapMode::Clamp: return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		}

		ASSERT_MSG(false, "Unkown wrapMode");
		return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	}

	struct Resource
	{
		ComPtr<ID3D12Resource> Resource;
		D3D12_RESOURCE_STATES CurrentState;

		std::vector<D3D12_RESOURCE_STATES> subResourceStates;
	};

	struct CommandBuffer
	{
		ComPtr<ID3D12CommandAllocator> CommandAllocator = nullptr;
		u64 FenceValue = 0;
		ComPtr<ID3D12Fence1> Fence = nullptr;
		u32 DeferredReleasesFlag;
		ComPtr<ID3D12GraphicsCommandList6> CommandList = nullptr;

		void Wait(HANDLE fenceEvent)
		{
			ASSERT(Fence && fenceEvent);

			if (Fence->GetCompletedValue() < FenceValue)
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
		RenderTarget_DX12()
		{
			for (u32 i = 0; i < 8; i++)
			{
				RenderTargetResources[i] = nullptr;
			}
		}
		Ref<Resource> RenderTargetResources[8];
		DescriptorHandle rtvs[8];
		
		Ref<Resource> DepthBufferResource = nullptr;
		DescriptorHandle dsv;

		D3D12_RECT rect;
		D3D12_VIEWPORT ViewPort;
		Device_DX12* device = nullptr;

		DescriptorHandle renderTargetSrvs[8];
		DescriptorHandle renderTargetUavs[8];
		DescriptorHandle depthStencilSrv;

		~RenderTarget_DX12();
	};

	struct GraphicsPipeline_DX12
	{
		ComPtr<ID3D12RootSignature> RootSignature;
		ComPtr<ID3D12PipelineState> PipelineState;
		Device_DX12* device = nullptr;

		std::vector<u32> TextureIndices;
		std::vector<u32> ConstantBufferIndices;

		~GraphicsPipeline_DX12();
	};

	struct ComputePipeline_DX12
	{
		ComPtr<ID3D12RootSignature> RootSignature;
		ComPtr<ID3D12PipelineState> PipelineState;
		Device_DX12* device = nullptr;

		std::vector<u32> TextureIndices;
		std::vector<u32> ConstantBufferIndices;
		std::vector<u32> UAVIndices;

		~ComputePipeline_DX12();
	};

	struct VertexBuffer_DX12
	{
		Ref<Resource> VertexBufferResource;
		D3D12_VERTEX_BUFFER_VIEW VertexBufferView;
		Device_DX12* device = nullptr;

		~VertexBuffer_DX12();
	};

	struct IndexBuffer_DX12
	{
		Ref<Resource> IndexBufferResource;
		D3D12_INDEX_BUFFER_VIEW IndexBufferView;
		Device_DX12* device = nullptr;

		~IndexBuffer_DX12();
	};

	struct ConstantBuffer_DX12
	{
		Ref<Resource> ConstantBufferResource;
		DescriptorHandle cbv;
		u64 MemorySize;
		void* GPUAddress;
		Device_DX12* device = nullptr;

		~ConstantBuffer_DX12();
	};
	
	struct Texture_DX12
	{
		Ref<Resource> TextureResource;
		DescriptorHandle srv;
		DescriptorHandle uav;

		std::vector<DescriptorHandle> mipSrvs;
		std::vector<DescriptorHandle> mipUavs;
		Device_DX12* device = nullptr;
		~Texture_DX12();
	};

	struct RaytracingPipeline_DX12
	{
		ComPtr<ID3D12RootSignature> RootSignature;
		ComPtr<ID3D12StateObject> StateObject;
		Device_DX12* device = nullptr;

		std::vector<u32> TextureIndices;
		std::vector<u32> ConstantBufferIndices;
		std::vector<u32> UAVIndices;
		u32 TLASSlot{ 0 };

		ComPtr<ID3D12Resource> RayGenShaderTable;
		ComPtr<ID3D12Resource> MissShaderTable;
		ComPtr<ID3D12Resource> HitGroupShaderTable;

	};

	struct BLAS_DX12
	{
		ComPtr<ID3D12Resource> Resource;
	};

	struct TLAS_DX12
	{
		ComPtr<ID3D12Resource> Resource;
	};

} }

#endif