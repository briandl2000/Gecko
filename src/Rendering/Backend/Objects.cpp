#include "Rendering/Backend/Objects.h"

#include <cmath>

namespace Gecko
{

	u32 CalculateNumberOfMips(u32 width, u32 height)
	{
		return 1 + static_cast<u32>(std::log2(static_cast<float>(std::max(width, height))));
	}

	u32 FormatSizeInBytes(const DataFormat& format)
	{
		switch (format)
		{
		case DataFormat::R8G8B8A8_SRGB: return 4 * 1;
		case DataFormat::R8G8B8A8_UNORM: return 4 * 1;
		case DataFormat::R32G32_FLOAT: return 4 * 2;
		case DataFormat::R32G32B32_FLOAT: return 4 * 3;
		case DataFormat::R32G32B32A32_FLOAT: return 4 * 4;
		case DataFormat::R16G16B16A16_FLOAT: return 4 * 2;

		case DataFormat::R32_FLOAT: return 4;

		case DataFormat::R8_UINT: return 1;
		case DataFormat::R16_UINT: return 2;
		case DataFormat::R32_UINT: return 4;

		case DataFormat::R8_INT: return 1;
		case DataFormat::R16_INT: return 2;
		case DataFormat::R32_INT: return 4;

		case DataFormat::None: break;
		}

		ASSERT(false, "Unknown Format");
		return 0;
	}

#pragma region EnumToString
	std::string EnumToString(ShaderType val)
	{
		switch (val)
		{
		case ShaderType::All:
			return "All";
		case ShaderType::Vertex:
			return "Vertex";
		case ShaderType::Pixel:
			return "Pixel";
		case ShaderType::Compute:
			return "Compute";
		default:
			return "Invalid ShaderType!";
		}
	}

	std::string EnumToString(DataFormat val)
	{
		switch (val)
		{
		case DataFormat::R8G8B8A8_SRGB:
			return "R8G8B8A8_SRGB";
		case DataFormat::R8G8B8A8_UNORM:
			return "R8G8B8A8_UNORM";
		case DataFormat::R32G32_FLOAT:
			return "R32G32_FLOAT";
		case DataFormat::R32G32B32_FLOAT:
			return "R32G32B32_FLOAT";
		case DataFormat::R32G32B32A32_FLOAT:
			return "R32G32B32A32_FLOAT";
		case DataFormat::R16G16B16A16_FLOAT:
			return "R16G16B16A16_FLOAT";

		case DataFormat::R32_FLOAT:
			return "R32_FLOAT";

		case DataFormat::R8_UINT:
			return "R8_UINT";
		case DataFormat::R16_UINT:
			return "R16_UINT";
		case DataFormat::R32_UINT:
			return "R32_UINT";

		case DataFormat::R8_INT:
			return "R8_INT";
		case DataFormat::R16_INT:
			return "R16_INT";
		case DataFormat::R32_INT:
			return "R32_INT";

		case DataFormat::None:
		default:
			return "Invalid DataFormat!";
		}
	}

	std::string EnumToString(ClearValueType val)
	{
		switch (val)
		{
		case ClearValueType::RenderTarget:
			return "RenderTarget";
		case ClearValueType::DepthStencil:
			return "DepthStencil";
		default:
			return "Invalid ClearValueType!";
		}
	}

	std::string EnumToString(TextureType val)
	{
		switch (val)
		{
		case TextureType::Tex1D:
			return "Tex1D";
		case TextureType::Tex2D:
			return "Tex2D";
		case TextureType::Tex3D:
			return "Tex3D";
		case TextureType::TexCube:
			return "TexCube";
		case TextureType::Tex1DArray:
			return "Tex1DArray";
		case TextureType::Tex2DArray:
			return "Tex2DArray";
		case TextureType::None:
		default:
			return "Invalid TextureType!";
		}
	}

	std::string EnumToString(CullMode val)
	{
		switch (val)
		{
			/* None is a valid cull mode */
		case CullMode::None:
			return "None";
		case CullMode::Back:
			return "Back";
		case CullMode::Front:
			return "Front";
		default:
			return "Invalid CullMode!";
		}
	}

	std::string EnumToString(WindingOrder val)
	{
		switch (val)
		{
		case WindingOrder::ClockWise:
			return "ClockWise";
		case WindingOrder::CounterClockWise:
			return "CounterClockWise";
		default:
			return "Invalid WindingOrder!";
		}
	}

	std::string EnumToString(PrimitiveType val)
	{
		switch (val)
		{
		case PrimitiveType::Lines:
			return "Lines";
		case PrimitiveType::Triangles:
			return "Triangles";
		default:
			return "Invalid PrimitiveType!";
		}
	}

	std::string EnumToString(SamplerFilter val)
	{
		switch (val)
		{
		case SamplerFilter::Linear:
			return "Linear";
		case SamplerFilter::Point:
			return "Point";
		default:
			return "Invalid SamplerFilter!";
		}
	}

	std::string EnumToString(SamplerWrapMode val)
	{
		switch (val)
		{
		case SamplerWrapMode::Wrap:
			return "Wrap";
		case SamplerWrapMode::Clamp:
			return "Clamp";
		default:
			return "Invalid SamplerWrapMode!";
		}
	}

	std::string EnumToString(ResourceType val)
	{
		switch (val)
		{
		case ResourceType::Texture:
			return "Texture";
		case ResourceType::ConstantBuffer:
			return "ConstantBuffer";
		case ResourceType::StructuredBuffer:
			return "StructuredBuffer";
		case ResourceType::LocalData:
			return "LocalData";
		case ResourceType::None:
		default:
			return "Invalid ResourceType!";
		}
	}

	std::string EnumToString(BufferType val)
	{
		switch (val)
		{
		case BufferType::Vertex:
			return "Vertex";
		case BufferType::Index:
			return "Index";
		case BufferType::Constant:
			return "Constant";
		case BufferType::Structured:
			return "Structured";
		case BufferType::None:
		default:
			return "Invalid BufferType!";
		}
	}

	std::string EnumToString(MemoryType val)
	{
		switch (val)
		{
		case MemoryType::Shared:
			return "Shared";
		case MemoryType::Dedicated:
			return "Dedicated";
		case MemoryType::None:
		default:
			return "Invalid GPU MemoryType!";
		}
	}
#pragma endregion

#pragma region Struct::IsValid

#pragma warning(push)
#pragma warning(disable: 4100) // Unreferenced formal parameter

	bool VertexAttribute::IsValid(std::string* failureReason) const
	{
		if (AttributeFormat == DataFormat::None)
		{
			if (failureReason)
				*failureReason = "None cannot be used as a DataFormat for a VertexAttribute!";
			return false;
		}
		if (Size == 0)
		{
			if (failureReason)
				*failureReason = "Size of VertexAttribute must be greater than 0!";
			return false;
		}
		return true;
	}

	bool VertexLayout::IsValid(std::string* failureReason) const
	{
		for (const VertexAttribute& attributte : Attributes)
		{
			if (!attributte.IsValid(failureReason))
				return false;
		}
		return true;
	}

	bool ClearValue::IsValid(std::string* failureReason) const
	{
		return true;
	}

	bool SamplerDesc::IsValid(std::string* failureReason) const
	{
		return true;
	}

	bool PipelineResource::IsValid(std::string* failureReason) const
	{
		if (Type == ResourceType::None)
		{
			if (failureReason)
				*failureReason = "PipelineResource is for invalid ResourceType!";
			return false;
		}
		if (Type == ResourceType::LocalData && (Size == 0 || Size % 4 != 0))
		{
			if (failureReason && Size == 0)
				*failureReason = "LocalData cannot be empty!";
			else if (failureReason && Size % 4 != 0)
				*failureReason = "LocalData must be a multiple of 4 bytes (or 32 bits)!";
			return false;
		}
		return true;
	}

	bool VertexBufferDesc::IsValid(std::string* failureReason) const
	{
		if (Layout.Attributes.size() == 0)
		{
			if (failureReason)
				*failureReason = "VertexLayouts must be defined!";
			return false;
		}
		if (!Layout.IsValid(failureReason))
		{
			// failureReason gets filled in by Layout.IsValid()
			return false;
		}
		if (NumVertices == 0)
		{
			if (failureReason)
				*failureReason = "Cannot create VertexBuffer for 0 vertices!";
			return false;
		}
		return true;
	}

	bool IndexBufferDesc::IsValid(std::string* failureReason) const
	{
		if (IndexFormat != DataFormat::R16_UINT && IndexFormat != DataFormat::R32_UINT)
		{
			if (failureReason)
				*failureReason = "IndexFormat must be either R16_UINT or R32_UINT!";
			return false;
		}
		if (NumIndices == 0)
		{
			if (failureReason)
				*failureReason = "Cannot create IndexBuffer for 0 indices!";
			return false;
		}
		return true;
	}

	bool ConstantBufferDesc::IsValid(std::string* failureReason) const
	{
		if (Size == 0)
		{
			if (failureReason)
				*failureReason = "Cannot create ConstantBuffer of size 0!";
			return false;
		}
		return true;
	}

	bool StructuredBufferDesc::IsValid(std::string* failureReason) const
	{
		if (StructSize == 0 || NumElements == 0)
		{
			if (failureReason)
				*failureReason = "Cannot create StructuredBuffer of size 0!";
			return false;
		}
		return true;
	}

	bool BufferDesc::IsValid(std::string* failureReason) const
	{
		if (MemoryType == MemoryType::None)
		{
			if (failureReason)
				*failureReason = "Invalid memory type for buffer!";
			return false;
		}
		if (Type == BufferType::None)
		{
			if (failureReason)
				*failureReason = "Invalid buffer type!";
			return false;
		}
		if (MemoryType == MemoryType::Shared && CanReadWrite)
		{
			if (failureReason)
				*failureReason = "ReadWrite buffers must be in Dedicated Memory!";
			return false;
		}
		if (Type == BufferType::Constant)
		{
			if (Size == 0)
			{
				if (failureReason)
					*failureReason = "Cannot create ConstantBuffer of size 0!";
				return false;
			}
			if (CanReadWrite)
			{
				if (failureReason)
					*failureReason = "ConstantBuffer cannot be ReadWrite!";
				return false;
			}
			return true;
		}
		if (Stride == 0 || NumElements == 0)
		{
			if (failureReason)
				*failureReason = "Cannot create buffer of size 0!";
			return false;
		}
		return true;
	}

	bool Buffer::IsValid(std::string* failureReason) const
	{
		if (!Desc.IsValid(failureReason))
		{
			// failureReason is filled in by Desc.IsValid()
			return false;
		}
		if (!Data)
		{
			if (failureReason)
				*failureReason = "Data of Buffer cannot be empty!";
			return false;
		}
		return true;
	}

	bool TextureDesc::IsValid(std::string* failureReason) const
	{
		if (Format == DataFormat::None)
		{
			if (failureReason)
				*failureReason = "Invalid DataFormat for Texture!";
			return false;
		}
		if (Type == TextureType::None)
		{
			if (failureReason)
				*failureReason = "Invalid TextureType!";
			return false;
		}
		if ((Type == TextureType::Tex1D || Type == TextureType::Tex1DArray) &&
			!(Height == 1 && Depth == 1))
		{
			if (failureReason)
				*failureReason = "Height and Depth dimensions must be 1 for 1D textures!";
			return false;
		}
		else if ((Type == TextureType::Tex2D || Type == TextureType::Tex2DArray) &&
			!(Depth == 1))
		{
			if (failureReason)
				*failureReason = "Depth dimension must be 1 for 2D textures!";
			return false;
		}
		return true;
	}

	bool Texture::IsValid(std::string* failureReason) const
	{
		if (!Desc.IsValid(failureReason))
		{
			// failureReason filled in by Desc.IsValid()
			return false;
		}
		if (!Data)
		{
			if (failureReason)
				*failureReason = "Data of Texture cannot be empty!";
			return false;
		}
		return true;
	}

	bool RenderTargetDesc::IsValid(std::string* failureReason) const
	{
		if (Width == 0 || Height == 0)
		{
			if (failureReason)
				*failureReason = "Invalid dimensions for RenderTarget!";
			return false;
		}
		for (u32 i = 0; i < NumRenderTargets; i++)
		{
			if (RenderTargetFormats[i] == DataFormat::None)
			{
				if (failureReason)
					*failureReason = "Invalid format for render target " + std::to_string(i) + "!";
				return false;
			}
		}
		if (NumRenderTargets == 0 && DepthStencilFormat == DataFormat::None)
		{
			if (failureReason)
				*failureReason = "At least one valid RenderTarget or DepthStencil must be specified!";
			return false;
		}
		return true;
	}

	bool RenderTarget::IsValid(std::string* failureReason) const
	{
		if (!RenderTextures[0].IsValid(failureReason) && !DepthTexture.IsValid(failureReason))
		{
			// failureReason filled in by Texture.IsValid()
			return false;
		}
		if (!Data)
		{
			if (failureReason)
				*failureReason = "Data of RenderTarget cannot be empty!";
			return false;
		}
		return true;
	}

	bool GraphicsPipelineDesc::IsValid(std::string* failureReason) const
	{
		if (!VertexShaderPath)
		{
			if (failureReason)
				*failureReason = "GraphicsPipeline must have a valid VertexShader!";
			return false;
		}
		if (!ShaderVersion)
		{
			if (failureReason)
				*failureReason = "Invalid ShaderVersion for GraphicsPipeline!";
			return false;
		}

		for (u32 i = 0; i < NumRenderTargets; i++)
		{
			if (RenderTextureFormats[i] == DataFormat::None)
			{
				if (failureReason)
					*failureReason = "Render texture format of target (" + std::to_string(i) + ") is None, make sure it is set!";
				return false;
			}
		}
		if (NumRenderTargets == 0 && DepthStencilFormat == DataFormat::None)
		{
			if (failureReason)
				*failureReason = "Number of render targets is 0 and the depth stencil format is undefined, atleast one render target or the depth stencil need to be set!";
			return false;
		}

		if (DepthStencilFormat != DataFormat::None && (DepthStencilFormat != DataFormat::R32_FLOAT))
		{
			if (failureReason)
				*failureReason = "Depth stencil format must be of type R32_FLOAT!";
			return false;
		}
		return true;
	}

	bool GraphicsPipeline::IsValid(std::string* failureReason) const
	{
		if (!Desc.IsValid(failureReason))
		{
			// failureReason filled in by Desc.IsValid()
			return false;
		}
		if (!Data)
		{
			if (failureReason)
				*failureReason = "Data of GraphicsPipeline cannot be empty!";
			return false;
		}
		return true;
	}

	bool ComputePipelineDesc::IsValid(std::string* failureReason) const
	{
		if (!ComputeShaderPath)
		{
			if (failureReason)
				*failureReason = "GraphicsPipeline must have a valid VertexShader!";
			return false;
		}
		if (!ShaderVersion)
		{
			if (failureReason)
				*failureReason = "Invalid ShaderVersion for GraphicsPipeline!";
			return false;
		}

		return true;
	}

	bool ComputePipeline::IsValid(std::string* failureReason) const
	{
		if (!Desc.IsValid(failureReason))
		{
			// failureReason filled in by Desc.IsValid()
			return false;
		}
		if (!Data)
		{
			if (failureReason)
				*failureReason = "Data of ComputePipeline cannot be empty!";
			return false;
		}
		return true;
	}
#pragma warning(pop) // disabled 4100: unreferenced formal parameter
#pragma endregion
}
