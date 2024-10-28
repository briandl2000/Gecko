#pragma once
#include "Defines.h"

#include "Logging/Asserts.h"

#include <vector>

namespace Gecko {
	
	enum class ShaderType
	{
		All,
		Vertex,
		Pixel
	};

	enum class Format
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
		Pixel
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

	enum class BufferType
	{
		Constant,
		Structured
	};

	u32 FormatSizeInBytes(const Format& format);

	u32 GetRenderTargetResourceIndex(const RenderTargetType& renderTargetType);

	// TODO: find a better place for this
	u32 CalculateNumberOfMips(u32 width, u32 height);

	// TODO: add a resource structure.

	// Render target

	// TODO: make it so that the render target uses textures instead of thier own data.
	struct ClearValue
	{
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

		union
		{
			f32 Values[4] {0.};
			f32 Depth;
		};
	};

	struct RenderTargetDesc
	{
		RenderTargetDesc()
		{
			for (u32 i = 0; i < 8; i++)
			{
				RenderTargetFormats[i] = Format::None;
			}
		}

		ClearValue RenderTargetClearValues[8]{ ClearValueType::RenderTarget};
		ClearValue DepthTargetClearValue{ ClearValueType::DepthStencil };
		Format RenderTargetFormats[8]{ Format::None };
		Format DepthStencilFormat{ Format::None };
		u32 NumRenderTargets{ 0 };
		u32 Width{ 0 };
		u32 Height{ 0 };
		bool AllowRenderTargetTexture{ false };
		bool AllowDepthStencilTexture{ false };
	};

	struct RenderTarget
	{
		RenderTargetDesc Desc;
		Ref<void> Data;
	};

	// Vertex buffer

	struct VertexAttribute
	{
		VertexAttribute() = default;

		VertexAttribute(Format format, const char* name)
			: Name(name)
			, Format(format)
			, Size(FormatSizeInBytes(format))
			, Offset(0)
		{}

		const char* Name;
		Format Format;
		u32 Size;
		u32 Offset;

	};

	struct VertexLayout
	{
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

		std::vector<VertexAttribute> Attributes;
		u32 Stride = 0;

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
		VertexLayout Layout;
		void* VertexData{ nullptr };
		u32 NumVertices{ 0 };
	};

	struct VertexBuffer
	{
		VertexBufferDesc Desc;
		Ref<void> Data;
	};

	// Index buffer

	struct IndexBufferDesc // The Index buffer desc takes in format of the indices, the number of indices and the index data pointer. This pointer needs to stay valid until the CreateIndexBuffer function is called.
	{
		Format IndexFormat;
		u32 NumIndices{ 0 };
		void* IndexData{ nullptr };
	};

	struct IndexBuffer
	{
		IndexBufferDesc Desc;
		Ref<void> Data;
	};

	// Texture

	struct TextureDesc
	{
		Format Format{ Format::None };
		u32 Width{ 1 };
		u32 Height{ 1 };
		u32 Depth{ 1 };
		u32 NumMips{ 1 };
		u32 NumArraySlices{ 1 };
		TextureType Type;
	};

	struct Texture
	{
		TextureDesc Desc;
		Ref<void> Data;
	};

	// Constant Buffer

	struct ConstantBufferDesc
	{
		u64 Size{ 0 };
		ShaderVisibility Visibility{ ShaderVisibility::All };
	};

	struct ConstantBuffer
	{
		ConstantBufferDesc Desc;
		Ref<void> Data;
		void* Buffer;
	};

	// Graphics pipeline

	struct SamplerDesc
	{
		ShaderVisibility Visibility;
		SamplerFilter Filter{ SamplerFilter::Linear };
		SamplerWrapMode WrapU{ SamplerWrapMode::Wrap };
		SamplerWrapMode WrapV{ SamplerWrapMode::Wrap };
		SamplerWrapMode WrapW{ SamplerWrapMode::Wrap };
	};

	struct DynamicCallData
	{
		i32 BufferLocation{ -1 };
		u32 Size{ 0 };
		ShaderVisibility ConstantBufferVisibilities{ ShaderVisibility::All };
	};

	struct GraphicsPipelineDesc
	{
		GraphicsPipelineDesc() = default;

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
		const char* ShaderVersion{ "" };

		VertexLayout VertexLayout;

		Format RenderTargetFormats[8]{ Format::None };
		Format DepthStencilFormat{ Format::None };

		CullMode CullMode{ CullMode::None };
		WindingOrder WindingOrder{ WindingOrder::ClockWise };
		PrimitiveType PrimitiveType{ PrimitiveType::Triangles };
		
		std::vector<ShaderVisibility> ConstantBufferVisibilities;

		DynamicCallData DynamicCallData;

		std::vector<ShaderVisibility> TextureShaderVisibilities;

		std::vector<SamplerDesc> SamplerDescs;
		bool DepthBoundsTest = false;
	};

	struct GraphicsPipeline
	{
		GraphicsPipelineDesc Desc;
		Ref<void> Data;
	};

	// Compute pipeline

	struct ComputePipelineDesc
	{
		ComputePipelineDesc() = default;

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
		const char* ShaderVersion{ "" };
		// EntryPoint == the name of the entrypoint of the shader (DX12 allows for custom entrypoints), usually "main"
		const char* EntryPoint{ "main" };
		
		u32 NumConstantBuffers{ 0 };
		DynamicCallData DynamicCallData;
		u32 NumTextures{ 0 };
		u32 NumUAVs{ 0 };
		std::vector<SamplerDesc> SamplerDescs;
	};

	struct ComputePipeline
	{
		ComputePipelineDesc Desc;
		Ref<void> Data;
	};

	// TODO: make the Raytracing structures

	// Raytracing structures

	struct RaytracingPipelineDesc
	{
		RaytracingPipelineDesc() = default;

		const char* RaytraceShaderPath{ nullptr };

		u32 NumConstantBuffers{ 0 };
		DynamicCallData DynamicCallData;
		u32 NumTextures{ 0 };
		u32 NumUAVs{ 0 };

		std::vector<SamplerDesc> SamplerDescs;
	};

	struct RaytracingPipeline
	{
		RaytracingPipelineDesc Desc;
		Ref<void> Data;
	};

	struct BLASDesc
	{
		VertexBuffer VertexBuffer;
		IndexBuffer IndexBuffer;
	};

	struct BLAS
	{
		BLASDesc Desc;
		Ref<void> Data;
	};

	// TLAS

	struct BLASInstanceData
	{
		BLAS BLAS;
		glm::mat4 Transform;
	};

	struct TLASRefitDesc
	{
		std::vector<BLASInstanceData> BLASInstances;
	};
	
	struct TLAS
	{
		Ref<void> Data;
	};

	// TODO: Make is valid functions for each descriptor structure.

}