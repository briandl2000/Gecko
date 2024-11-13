#pragma once
#include "Defines.h"

#include "Core/Asserts.h"

#include <vector>

namespace Gecko {
	
	enum class ShaderType
	{
		All,
		Vertex,
		Pixel
	};

	enum class DataFormat
	{
		None,
		R8G8B8A8_SRGB,
		R8G8B8A8_UNORM,
		R32G32_FLOAT,
		R32G32B32_FLOAT,
		R32G32B32A32_FLOAT,
		R16G16B16A16_FLOAT,

		R32_FLOAT,

		R8_UINT,
		R16_UINT,
		R32_UINT,

		R8_INT,
		R16_INT,
		R32_INT,

	};

	enum class ClearValueType
	{
		RenderTarget,
		DepthStencil
	};

	enum class RenderTargetType
	{
		Target0,
		Target1,
		Target2,
		Target3,
		Target4,
		Target5,
		Target6,
		Target7,
		Target8,
		TargetDepth,
	};

	enum class TextureType
	{
		None,
		Tex1D,
		Tex2D,
		Tex3D,
		TexCube,
		Tex1DArray,
		Tex2DArray
	};

	enum class ShaderVisibility
	{
		All,
		Vertex,
		Pixel,
		Compute
	};

	enum class CullMode
	{
		None,
		Back,
		Front
	};

	enum class WindingOrder
	{
		ClockWise,
		CounterClockWise
	};

	enum class PrimitiveType
	{
		Lines,
		Triangles
	};

	enum class SamplerFilter
	{
		Linear,
		Point
	};

	enum class SamplerWrapMode
	{
		Wrap,
		Clamp
	};

	enum class ResourceType
	{
		None,
		Texture,
		ConstantBuffer,
		StructuredBuffer,
		LocalData
	};

	enum class BufferType
	{
		None,
		Vertex,
		Index,
		Constant, 
		Structured
	};

	u32 FormatSizeInBytes(const DataFormat& format);

	//u32 GetRenderTargetResourceIndex(const RenderTargetType& renderTargetType);

	// TODO: find a better place for this
	u32 CalculateNumberOfMips(u32 width, u32 height);

	// TODO: add a resource structure.

	// Vertex buffer

	struct VertexAttribute
	{
		const char* Name{ "VertexAttribute" };
		DataFormat AttributeFormat{ DataFormat::None };
		u32 Size{ 0 };
		u32 Offset{ 0 };
		
		VertexAttribute() = default;
		VertexAttribute(DataFormat format, const char* name)
			: Name(name)
			, AttributeFormat(format)
			, Size(FormatSizeInBytes(format))
			, Offset(0)
		{}

		bool IsValid() const
		{
			return AttributeFormat != DataFormat::None && Size > 0;
		}
		operator bool() const
		{
			return IsValid();
		}

		// Does not check for name equality
		bool operator==(const VertexAttribute& other) const
		{
			return AttributeFormat == other.AttributeFormat && Size == other.Size && Offset == other.Offset;
		}
		bool operator!=(const VertexAttribute& other) const
		{
			return !(*this == other);
		}
	};

	struct VertexLayout
	{
		std::vector<VertexAttribute> Attributes{};
		u32 Stride{ 0 };

		VertexLayout() = default;
		VertexLayout(const std::initializer_list<VertexAttribute> attributes)
			: Attributes(attributes)
		{
			CalculateOffsetsAndStride();
		};
		VertexLayout(const std::vector<VertexAttribute> attributes)
			: Attributes(attributes)
		{
			CalculateOffsetsAndStride();
		};
		~VertexLayout() {};

		bool IsValid() const
		{
			return true;
		}
		operator bool() const
		{
			return IsValid();
		}

		bool operator==(const VertexLayout& other) const
		{
			if (Stride != other.Stride || Attributes.size() != other.Attributes.size())
			{
				return false;
			}
			for (size_t i = 0; i < Attributes.size(); ++i)
			{
				if (Attributes[i] != other.Attributes[i])
				{
					return false;
				}
			}
			return true;
		}
		bool operator!=(const VertexLayout& other) const
		{
			return !(*this == other);
		}

	private:
		void CalculateOffsetsAndStride()
		{
			u32 offset = 0;
			Stride = 0;
			for (VertexAttribute& attributte : Attributes)
			{
				attributte.Offset = offset;
				offset += attributte.Size;
				Stride += attributte.Size;
			}
		}
	};

	struct VertexBufferDesc // The Vertex buffer desc takes in a layout of the vertex and the raw vertex data pointer. This pointer needs to stay valid until the CreateVertexBuffer function is called.
	{
		VertexLayout Layout{ VertexLayout() };
		void* VertexData{ nullptr };
		u32 NumVertices{ 0 };

		bool IsValid() const
		{
			if (Layout.Stride == 0 || Layout.Attributes.empty())
				return false;

			if (!VertexData)
				return false;

			if (!NumVertices)
				return false;

			return true;
		}
		operator bool() const
		{
			return IsValid();
		}
	};

	struct VertexBuffer
	{
		VertexBufferDesc Desc{};
		Ref<void> Data{ nullptr };

		bool IsValid() const
		{
			return Desc.IsValid() && Data;
		}
		operator bool() const
		{
			return IsValid();
		}
	};

	// Index buffer

	struct IndexBufferDesc // The Index buffer desc takes in format of the indices, the number of indices and the index data pointer. This pointer needs to stay valid until the CreateIndexBuffer function is called.
	{
		DataFormat IndexFormat{ DataFormat::None };
		u32 NumIndices{ 0 };
		void* IndexData{ nullptr };

		bool IsValid() const
		{
			// IndexBuffer format should always be R16_UINT or R32_UINT
			if (IndexFormat != DataFormat::R16_UINT && IndexFormat != DataFormat::R32_UINT)
				return false;

			return NumIndices != 0 && IndexData != nullptr;
		}
		operator bool() const
		{
			return IsValid();
		}
	};

	struct IndexBuffer
	{
		IndexBufferDesc Desc{};
		Ref<void> Data{ nullptr };

		bool IsValid() const
		{
			return Desc.IsValid() && Data;
		}
		operator bool() const
		{
			return IsValid();
		}
	};


	// Constant Buffer

	struct ConstantBufferDesc
	{
		u64 Size{ 0 };
	
		bool IsValid() const
		{
			return Size > 0;
		}
		operator bool() const
		{
			return IsValid();
		}
	};

	struct Buffer
	{
		BufferType Type{ BufferType::None };
		ConstantBufferDesc Desc{};
		Ref<void> Data{ nullptr };
		
		bool IsValid() const
		{
			// You can decide not to use Buffer if you feel like you don't need the void* data
			return Desc.IsValid() && Data;
		}
		operator bool() const
		{
			return IsValid();
		}
	};

	// Texture

	struct TextureDesc
	{
		DataFormat Format{ DataFormat::None };
		u32 Width{ 1 };
		u32 Height{ 1 };
		u32 Depth{ 1 };
		u32 NumMips{ 1 };
		u32 NumArraySlices{ 1 };
		TextureType Type{ TextureType::None };

		bool IsValid() const
		{
			if (Format == DataFormat::None || Type == TextureType::None)
				return false;

			if (Type == TextureType::Tex1D || Type == TextureType::Tex1DArray)
				return Height == 1 && Depth == 1;
			else if (Type == TextureType::Tex2D || Type == TextureType::Tex2DArray)
				return Depth == 1;

			return true;
		}
		operator bool() const
		{
			return IsValid();
		}

	};

	struct Texture
	{
		TextureDesc Desc{};
		Ref<void> Data{ nullptr };

		bool IsValid() const
		{
			return Desc.IsValid() && Data;
		}
		operator bool() const
		{
			return IsValid();
		}
	};

	// Render target

	struct ClearValue
	{
		union
		{
			f32 Values[4]{ 0. };
			f32 Depth;
		};

		ClearValue(ClearValueType type = ClearValueType::RenderTarget)
		{
			switch (type)
			{
			case ClearValueType::RenderTarget:
				Values[0] = 0.f;
				Values[1] = 0.f;
				Values[2] = 0.f;
				Values[3] = 1.f;
				break;
			case ClearValueType::DepthStencil:
				Depth = 1.f;
				break;
			}
		}

		ClearValue(f32 r, f32 g, f32 b, f32 a)
		{
			Values[0] = r;
			Values[1] = g;
			Values[2] = b;
			Values[3] = a;
		}

		bool IsValid() const
		{
			return true;
		}
		operator bool() const
		{
			return IsValid();
		}

		bool operator==(const ClearValue& other) const
		{
			for (u32 i = 0; i < 4; ++i)
			{
				if (Values[i] != other.Values[i])
					return false;
			}

			return true;
		}
		bool operator!=(const ClearValue& other) const
		{
			return !(*this == other);
		}
	};

	struct RenderTargetDesc
	{
		ClearValue RenderTargetClearValues[8]{ ClearValueType::RenderTarget };
		ClearValue DepthTargetClearValue{ ClearValueType::DepthStencil };
		DataFormat RenderTargetFormats[8]{
			DataFormat::None, DataFormat::None, DataFormat::None, DataFormat::None,
			DataFormat::None, DataFormat::None, DataFormat::None, DataFormat::None
		};
		DataFormat DepthStencilFormat{ DataFormat::None };
		u32 NumRenderTargets{ 0 };
		u32 NumMips[8]{ 1, 1, 1, 1, 1, 1, 1, 1 };
		u32 DepthMips{ 1 };
		u32 Width{ 0 };
		u32 Height{ 0 };

		bool IsValid() const
		{
			// RenderTarget needs to have either at least one valid RenderTexture or a valid DepthTexture (or both)
			// Assume that if the first RenderTexture does not have a valid format, none of them do
			if (RenderTargetFormats[0] == DataFormat::None && DepthStencilFormat == DataFormat::None)
				return false;

			if (Width == 0 || Height == 0)
				return false;

			return true;
		}
		operator bool() const
		{
			return IsValid();
		}
	};

	struct RenderTarget
	{
		RenderTargetDesc Desc{};
		Texture RenderTextures[8]{
			Texture(), Texture(), Texture(), Texture(),
			Texture(), Texture(), Texture(), Texture()
		};
		Texture DepthTexture{ Texture() };
		Ref<void> Data{ nullptr };

		bool IsValid() const
		{
			// RenderTarget needs to have either at least one valid RenderTexture or a valid DepthTexture (or both)
			// Assume that if the first RenderTexture is invalid, all of them are
			if (!RenderTextures[0].IsValid() && !DepthTexture.IsValid())
				return false;

			return Desc.IsValid() && Data;
		}
		operator bool() const
		{
			return IsValid();
		}
	};

	// Graphics pipeline

	struct SamplerDesc
	{
		ShaderVisibility Visibility{ ShaderVisibility::All };
		SamplerFilter Filter{ SamplerFilter::Linear };
		SamplerWrapMode WrapU{ SamplerWrapMode::Wrap };
		SamplerWrapMode WrapV{ SamplerWrapMode::Wrap };
		SamplerWrapMode WrapW{ SamplerWrapMode::Wrap };

		bool IsValid() const
		{
			return true;
		}
		operator bool() const
		{
			return IsValid();
		}

		bool operator==(const SamplerDesc& other) const
		{
			return Visibility == other.Visibility && Filter == other.Filter &&
				WrapU == other.WrapU && WrapV == other.WrapV && WrapW == other.WrapW;
		}
		bool operator!=(const SamplerDesc& other) const
		{
			return !(*this == other);
		}
	};

	struct DynamicCallData
	{
		i32 BufferLocation{ -1 };
		u32 Size{ 0 };
		ShaderVisibility ConstantBufferVisibilities{ ShaderVisibility::All };

		bool IsValid() const
		{
			return BufferLocation > -1 && Size > 0;
		}
		operator bool() const
		{
			return IsValid();
		}
	};

	struct PipelineResource
	{
		static PipelineResource Texture(ShaderVisibility visibility, u32 bindLocation)
		{
			return {ResourceType::Texture, visibility, bindLocation, 0};
		}
		static PipelineResource ConstantBuffer(ShaderVisibility visibility, u32 bindLocation)
		{
			return {ResourceType::ConstantBuffer, visibility, bindLocation, 0};
		}
		static PipelineResource StructuredBuffer(ShaderVisibility visibility, u32 bindLocation)
		{
			return {ResourceType::StructuredBuffer, visibility, bindLocation, 0};
		}
		static PipelineResource LocalData(ShaderVisibility visibility, u32 bindLocation, u32 size)
		{
			return {ResourceType::LocalData, visibility, bindLocation, size};
		}

		ResourceType Type{ ResourceType::None };
		ShaderVisibility Visibility{ ShaderVisibility::All };
		u32 BindLocation{ 0 };
		u32 Size{ 0 };

		bool IsValid() const
		{
			if(Type == ResourceType::None)
			{
				return false;
			}
			if(Type == ResourceType::LocalData && (Size == 0 || Size % 4 != 0))
			{
				return false;
			}
			return true;
		}
		operator bool() const
		{
			return IsValid();
		}
	};

	struct GraphicsPipelineDesc
	{
		const char* VertexShaderPath{ nullptr };
		// EntryPoint == the name of the entrypoint of the shader (DX12 allows for custom entrypoints), usually "main"
		const char* VertexEntryPoint{ "main" };

		const char* PixelShaderPath{ nullptr };
		const char* PixelEntryPoint{ "main" };

		/* ShaderVersion == shader language version that the shader uses, formatted as "type_X_Y";
			where type == vs for vertex shaders, ps for pixel shaders (type_ prefix gets added automatically in Device_DX12.cpp);
			X == main version (e.g. 5 for hlsl shader language 5.1);
			Y == subversion (e.g. 1 for 5.1);
			Users should usually initialise this as pipelineDesc.ShaderVersion = "5_1" or something similar, unless they want to
			use a shader type that has no support built into this library (so anything other than vertex, pixel or compute shaders).
			See for example https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/specifying-compiler-targets for D3D12
			*/
		const char* ShaderVersion{ nullptr };

		VertexLayout VertexLayout{};

		DataFormat RenderTextureFormats[8]{ DataFormat::None };
		DataFormat DepthStencilFormat{ DataFormat::None };

		CullMode CullMode{ CullMode::None };
		WindingOrder WindingOrder{ WindingOrder::ClockWise };
		PrimitiveType PrimitiveType{ PrimitiveType::Triangles };

		std::vector<PipelineResource> PipelineResources{};

		std::vector<SamplerDesc> SamplerDescs{};
		bool DepthBoundsTest{ false };

		bool IsValid() const
		{
			if (!VertexShaderPath || !ShaderVersion)
				return false;
			// RenderTarget needs to have either at least one valid RenderTexture or a valid DepthTexture (or both)
			// Assume that if the first RenderTexture does not have a valid format, none of them do
			if (RenderTextureFormats[0] == DataFormat::None && DepthStencilFormat == DataFormat::None)
				return false;

			return true;
		}
		operator bool() const
		{
			return IsValid();
		}
	};

	struct GraphicsPipeline
	{
		GraphicsPipelineDesc Desc{};
		Ref<void> Data{ nullptr };

		bool IsValid() const
		{
			return Desc.IsValid() && Data;
		}
		operator bool() const
		{
			return IsValid();
		}
	};

	// Compute pipeline

	struct ComputePipelineDesc
	{
		// Path from executable to shader object (source or compiled)
		const char* ComputeShaderPath{ nullptr };
		/* ShaderVersion == shader language version that the shader uses, formatted as "type_X_Y",
			where type == cs for compute shaders (type_ prefix gets added automatically in Device_DX12.cpp),
			X == main version (e.g. 5 for hlsl shader language 5.1),
			Y == subversion (e.g. 1 for 5.1).
			Users should usually initialise this as pipelineDesc.ShaderVersion = "5_1" or something similar, unless they want to
			use a shader type that has no support built into this library (so anything other than vertex, pixel or compute shaders)
			See for example https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/specifying-compiler-targets for D3D12
			*/
		const char* ShaderVersion{ nullptr };
		// EntryPoint == the name of the entrypoint of the shader (DX12 allows for custom entrypoints), usually "main"
		const char* EntryPoint{ "main" };

		std::vector<PipelineResource> PipelineReadOnlyResources{};
		std::vector<PipelineResource> PipelineReadWriteResources{};
		std::vector<SamplerDesc> SamplerDescs{};

		bool IsValid() const
		{
			if (!ComputeShaderPath || !ShaderVersion)
				return false;

			return true;
		}
		operator bool() const
		{
			return IsValid();
		}
	};

	struct ComputePipeline
	{
		ComputePipelineDesc Desc{};
		Ref<void> Data{ nullptr };

		bool IsValid() const
		{
			return Desc.IsValid() && Data;
		}
		operator bool() const
		{
			return IsValid();
		}
	};

	// TODO: Make is valid functions for each descriptor structure.

}
