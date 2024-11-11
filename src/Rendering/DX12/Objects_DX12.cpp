#ifdef DIRECTX_12
#include "Rendering/DX12/Objects_DX12.h"

#include "Rendering/DX12/Device_DX12.h"

namespace Gecko { namespace DX12 {

	
	DXGI_FORMAT FormatToD3D12Format(DataFormat format)
	{
		switch (format)
		{
		case DataFormat::R8G8B8A8_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		case DataFormat::R8G8B8A8_UNORM: return DXGI_FORMAT_R8G8B8A8_UNORM;
		
		case DataFormat::R32_FLOAT: return DXGI_FORMAT_R32_FLOAT;
		case DataFormat::R32G32_FLOAT: return DXGI_FORMAT_R32G32_FLOAT;
		case DataFormat::R32G32B32_FLOAT: return DXGI_FORMAT_R32G32B32_FLOAT;
		case DataFormat::R32G32B32A32_FLOAT: return DXGI_FORMAT_R32G32B32A32_FLOAT;
		case DataFormat::R16G16B16A16_FLOAT: return DXGI_FORMAT_R16G16B16A16_FLOAT;

		case DataFormat::R8_UINT: return DXGI_FORMAT_R8_UINT;
		case DataFormat::R16_UINT: return DXGI_FORMAT_R16_UINT;
		case DataFormat::R32_UINT: return DXGI_FORMAT_R32_UINT;

		case DataFormat::R8_INT: return DXGI_FORMAT_R8_SINT;
		case DataFormat::R16_INT: return DXGI_FORMAT_R16_SINT;
		case DataFormat::R32_INT: return DXGI_FORMAT_R32_SINT;

		case DataFormat::None: break;
		}

		ASSERT_MSG(false, "Unkown Format state.");
		return DXGI_FORMAT_UNKNOWN;
	}

	D3D12_SHADER_VISIBILITY ShaderVisibilityToD3D12ShaderVisibility(ShaderVisibility visibility)
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

	D3D12_CULL_MODE CullModeToD3D12CullMode(CullMode cullMode)
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

	D3D12_FILL_MODE PrimitiveTypeToD3D12FillMode(PrimitiveType type)
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

	D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTypeToD3D12PrimitiveTopologyType(PrimitiveType type)
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

	D3D_PRIMITIVE_TOPOLOGY PrimitiveTypeToD3D12PrimitiveTopology(PrimitiveType type)
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

	D3D12_SRV_DIMENSION TextureTypeToD3D12SrvDimension(TextureType textureType)
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

	D3D12_UAV_DIMENSION TextureTypeToD3D12UavDimension(TextureType textureType)
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

	D3D12_FILTER SamplerFilterToD3D12Filter(SamplerFilter samplerFilter)
	{
		switch (samplerFilter)
		{
		case SamplerFilter::Linear: return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
		case SamplerFilter::Point: return D3D12_FILTER_MIN_MAG_MIP_POINT;
		}

		ASSERT_MSG(false, "Unkown samplerFilter");
		return D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	}

	D3D12_TEXTURE_ADDRESS_MODE AddressModeToD3D12AddressMode(SamplerWrapMode wrapMode)
	{
		switch (wrapMode)
		{
		case SamplerWrapMode::Wrap: return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
		case SamplerWrapMode::Clamp: return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
		}

		ASSERT_MSG(false, "Unkown wrapMode");
		return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	}

	RenderTarget_DX12::~RenderTarget_DX12()
	{
		for (u32 i = 0; i < 8; i++)
		{
			if (RenderTargetViews[i].IsValid())
			{
				Device_DX12::FlagRtvDescriptorHandleForDeletion(RenderTargetViews[i]);
			}
		}
		if (DepthStencilView.IsValid())
		{
			Device_DX12::FlagDsvDescriptorHandleForDeletion(DepthStencilView);
		}
	}

	GraphicsPipeline_DX12::~GraphicsPipeline_DX12()
	{
		Device_DX12::FlagPipelineStateForDeletion(PipelineState);
		RootSignature = nullptr;
		PipelineState = nullptr;
	}

	ComputePipeline_DX12::~ComputePipeline_DX12()
	{
		Device_DX12::FlagPipelineStateForDeletion(PipelineState);
		RootSignature = nullptr;
		PipelineState = nullptr;
	}

	CommandBuffer::~CommandBuffer()
	{
		CommandAllocator = nullptr;
		Fence = nullptr;
		CommandList = nullptr;
	}

	VertexBuffer_DX12::~VertexBuffer_DX12()
	{
		Device_DX12::FlagResrouceForDeletion(VertexBufferResource);
	}

	IndexBuffer_DX12::~IndexBuffer_DX12()
	{
		Device_DX12::FlagResrouceForDeletion(IndexBufferResource);
	}

	ConstantBuffer_DX12::~ConstantBuffer_DX12()
	{
		Device_DX12::FlagResrouceForDeletion(ConstantBufferResource);
		Device_DX12::FlagSrvDescriptorHandleForDeletion(ConstantBufferView);
	}

	Texture_DX12::~Texture_DX12()
	{
		Device_DX12::FlagResrouceForDeletion(TextureResource);
		Device_DX12::FlagSrvDescriptorHandleForDeletion(ShaderResourceView);
		Device_DX12::FlagSrvDescriptorHandleForDeletion(UnorderedAccessView);

		for (u32 i = 0; i < MipShaderResourceViews.size(); i++)
		{
			Device_DX12::FlagSrvDescriptorHandleForDeletion(MipShaderResourceViews[i]);
		}
		for (u32 i = 0; i < MipUnorderedAccessViews.size(); i++)
		{
			Device_DX12::FlagSrvDescriptorHandleForDeletion(MipUnorderedAccessViews[i]);
		}
	}


} }

#endif