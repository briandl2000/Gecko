#include "Rendering/Frontend/ResourceManager/ResourceManager.h"

#include "Rendering/Backend/CommandList.h"

#include <stb_image.h>

namespace Gecko
{

	struct MipGenerationData
	{
		u32 Mip;
		u32 Srgb;
		u32 Width;
		u32 Height;
	};

	void ResourceManager::Init(Device* device)
	{
		m_Device = device;
		m_CurrentMeshIndex = 0;
		m_CurrentTextureIndex = 0;
		m_CurrentMaterialIndex = 0;
		m_CurrentRenderTargetIndex = 0;

		// MipMap Compute Pipeline
		{
			ComputePipelineDesc computePipelineDesc;
			computePipelineDesc.ComputeShaderPath = "Shaders/DownSample.gsh";
			computePipelineDesc.ShaderVersion = "5_1";
			computePipelineDesc.PipelineReadOnlyResources = 
			{
				PipelineResource::LocalData(ShaderType::Compute, 0, sizeof(MipGenerationData)), 
				PipelineResource::Texture(ShaderType::Compute, 0)
			};
			computePipelineDesc.PipelineReadWriteResources = 
			{
				PipelineResource::Texture(ShaderType::Compute, 0)
			};
			computePipelineDesc.SamplerDescs = {
				{ShaderType::Pixel, SamplerFilter::Linear}
			};

			DownsamplePipelineHandle = CreateComputePipeline(computePipelineDesc);
		}

		// HDR to Cubemap Compute Pipeline
		{
			ComputePipelineDesc computePipelineDesc;
			computePipelineDesc.ComputeShaderPath = "Shaders/HDR_to_cube.gsh";
			computePipelineDesc.ShaderVersion = "5_1";
			computePipelineDesc.SamplerDescs = {
				{ShaderType::Pixel, SamplerFilter::Linear, SamplerWrapMode::Wrap, SamplerWrapMode::Wrap, SamplerWrapMode::Wrap}
			};
			computePipelineDesc.PipelineReadOnlyResources = 
			{
				PipelineResource::Texture(ShaderType::Compute, 0)
			};
			computePipelineDesc.PipelineReadWriteResources = 
			{
				PipelineResource::Texture(ShaderType::Compute, 0)
			};

			HDRToCubeHandle = CreateComputePipeline(computePipelineDesc);
		}

		// IradianceMap Compute Pipeline
		{
			ComputePipelineDesc computePipelineDesc;
			computePipelineDesc.ComputeShaderPath = "Shaders/IrradianceMap.gsh";
			computePipelineDesc.ShaderVersion = "5_1";
			computePipelineDesc.SamplerDescs = {
				{ShaderType::Pixel, SamplerFilter::Linear, SamplerWrapMode::Clamp, SamplerWrapMode::Clamp, SamplerWrapMode::Clamp}
			};
			computePipelineDesc.PipelineReadOnlyResources = 
			{
				PipelineResource::Texture(ShaderType::Compute, 0)
			};
			computePipelineDesc.PipelineReadWriteResources = 
			{
				PipelineResource::Texture(ShaderType::Compute, 0)
			};

			IrradianceMapHandle = CreateComputePipeline(computePipelineDesc);

		}

		// Create missing Mesh
		{
			Vertex3D vertices[] = {
				{{ 1.f,  1.f, -1.f}, {1.f, 1.f}, { 0.f,  1.f,  0.f}, {1.f, 0.f, 0.f, 0.f}},
				{{ 1.f,  1.f,  1.f}, {1.f, 0.f}, { 0.f,  1.f,  0.f}, {1.f, 0.f, 0.f, 0.f}},
				{{-1.f,  1.f, -1.f}, {0.f, 1.f}, { 0.f,  1.f,  0.f}, {1.f, 0.f, 0.f, 0.f}},
				{{-1.f,  1.f,  1.f}, {0.f, 0.f}, { 0.f,  1.f,  0.f}, {1.f, 0.f, 0.f, 0.f}},
						   
				{{ 1.f,  1.f, -1.f}, {0.f, 1.f}, { 0.f,  0.f, -1.f}, {0.f, 1.f, 0.f, 0.f}},
				{{ 1.f, -1.f, -1.f}, {0.f, 0.f}, { 0.f,  0.f, -1.f}, {0.f, 1.f, 0.f, 0.f}},
				{{-1.f,  1.f, -1.f}, {1.f, 1.f}, { 0.f,  0.f, -1.f}, {0.f, 1.f, 0.f, 0.f}},
				{{-1.f, -1.f, -1.f}, {1.f, 0.f}, { 0.f,  0.f, -1.f}, {0.f, 1.f, 0.f, 0.f}},
						   
				{{-1.f,  1.f, -1.f}, {0.f, 1.f}, {-1.f,  0.f,  0.f}, {0.f, 1.f, 0.f, 0.f}},
				{{-1.f, -1.f, -1.f}, {0.f, 0.f}, {-1.f,  0.f,  0.f}, {0.f, 1.f, 0.f, 0.f}},
				{{-1.f,  1.f,  1.f}, {1.f, 1.f}, {-1.f,  0.f,  0.f}, {0.f, 1.f, 0.f, 0.f}},
				{{-1.f, -1.f,  1.f}, {1.f, 0.f}, {-1.f,  0.f,  0.f}, {0.f, 1.f, 0.f, 0.f}},
						   
				{{ 1.f,  1.f, -1.f}, {0.f, 0.f}, { 1.f,  0.f,  0.f}, {0.f, 1.f, 0.f, 0.f}},
				{{ 1.f, -1.f, -1.f}, {0.f, 1.f}, { 1.f,  0.f,  0.f}, {0.f, 1.f, 0.f, 0.f}},
				{{ 1.f,  1.f,  1.f}, {1.f, 0.f}, { 1.f,  0.f,  0.f}, {0.f, 1.f, 0.f, 0.f}},
				{{ 1.f, -1.f,  1.f}, {1.f, 1.f}, { 1.f,  0.f,  0.f}, {0.f, 1.f, 0.f, 0.f}},
						   
				{{ 1.f,  1.f,  1.f}, {0.f, 0.f}, { 0.f,  0.f,  1.f}, {0.f, 1.f, 0.f, 0.f}},
				{{ 1.f, -1.f,  1.f}, {0.f, 1.f}, { 0.f,  0.f,  1.f}, {0.f, 1.f, 0.f, 0.f}},
				{{-1.f,  1.f,  1.f}, {1.f, 0.f}, { 0.f,  0.f,  1.f}, {0.f, 1.f, 0.f, 0.f}},
				{{-1.f, -1.f,  1.f}, {1.f, 1.f}, { 0.f,  0.f,  1.f}, {0.f, 1.f, 0.f, 0.f}},

				{{ 1.f, -1.f, -1.f}, {0.f, 1.f}, { 0.f, -1.f,  0.f}, {1.f, 0.f, 0.f, 0.f}},
				{{ 1.f, -1.f,  1.f}, {0.f, 0.f}, { 0.f, -1.f,  0.f}, {1.f, 0.f, 0.f, 0.f}},
				{{-1.f, -1.f, -1.f}, {1.f, 1.f}, { 0.f, -1.f,  0.f}, {1.f, 0.f, 0.f, 0.f}},
				{{-1.f, -1.f,  1.f}, {1.f, 0.f}, { 0.f, -1.f,  0.f}, {1.f, 0.f, 0.f, 0.f}}
			};

			u32 indices[] = {
				1, 0, 3,
				3, 0, 2,

				0+4, 1+4, 3+4,
				0+4, 3+4, 2+4,

				0+8, 1+8, 3+8,
				0+8, 3+8, 2+8,

				1+12, 0+12, 3+12,
				3+12, 0+12, 2+12,

				1+16, 0+16, 3+16,
				3+16, 0+16, 2+16,

				0+20, 1+20, 3+20,
				0+20, 3+20, 2+20,
			};

			VertexBufferDesc vertexDesc;
			vertexDesc.Layout = Vertex3D::GetLayout();
			vertexDesc.NumVertices = 24;
			vertexDesc.MemoryType = MemoryType::Dedicated;

			IndexBufferDesc indexDesc;
			indexDesc.IndexFormat = DataFormat::R32_UINT;
			indexDesc.NumIndices = 36;
			indexDesc.MemoryType = MemoryType::Dedicated;

			m_CubeMeshHandle = CreateMesh(vertexDesc, indexDesc, vertices, indices);
		}

		// Create missing Texture
		{
			TextureDesc textureDesc;
			textureDesc.Width = 512;
			textureDesc.Height = 512;
			textureDesc.Type = TextureType::Tex2D;
			textureDesc.Format = DataFormat::R8G8B8A8_SRGB;
			textureDesc.NumMips = CalculateNumberOfMips(textureDesc.Width, textureDesc.Height);
			textureDesc.NumArraySlices = 1;

			std::vector<u32> emptyTexData;
			emptyTexData.resize(textureDesc.Width * textureDesc.Height);

			u32 ColorA = 0xFF8a817d;
			u32 ColorB = 0xFFf54e0c;

			for (u32 x = 0; x < textureDesc.Width; x++)
			{
				for (u32 y = 0; y < textureDesc.Height; y++)
				{
					u32 index = x + y * textureDesc.Width;
					if (((x / 8) + (y / 8)) % 2 == 0)
					{
						emptyTexData[index] = ColorA;
					}
					else
					{
						emptyTexData[index] = ColorB;
					}
				}
			}

			m_MissingTextureHandle = CreateTexture(textureDesc, emptyTexData.data(), true);
		}
		
		// Create missing Material
		{
			m_MissingMaterialHandle = CreateMaterial();
			Gecko::Buffer materialConstantBuffer = GetMaterial(m_MissingMaterialHandle).MaterialConstantBuffer;
			MaterialData materialData{ };// = reinterpret_cast<MaterialData*>(materialConstantBuffer._Buffer);
			materialData.materialTextureFlags |= 0b00001;
			m_Device->UploadBufferData(materialConstantBuffer, &materialData, sizeof(MaterialData));
		}

		SceneDataBuffer.resize(m_Device->GetNumBackBuffers());
		SceneData.resize(m_Device->GetNumBackBuffers());
		for(u32 i = 0; i < m_Device->GetNumBackBuffers(); i++)
		{
			Gecko::ConstantBufferDesc bufferDesc = {
				sizeof(SceneDataStruct)
			};
			bufferDesc.MemoryType = MemoryType::Shared;
			SceneDataBuffer[i] = m_Device->CreateConstantBuffer(bufferDesc);
			SceneData[i].CameraPosition = glm::vec3(0., 0., 2.);
			SceneData[i].ProjectionMatrix = glm::perspective(glm::radians(90.f), Gecko::Platform::GetScreenAspectRatio(), 0.1f, 100.f);

			SceneData[i].ViewMatrix = glm::translate(glm::mat4(1.), SceneData[i].CameraPosition);
			SceneData[i].invProjectionMatrix = glm::inverse(SceneData[i].ProjectionMatrix);
			SceneData[i].InvViewMatrix = glm::inverse(SceneData[i].ViewMatrix);
			SceneData[i].ViewOrientation = glm::mat4(glm::mat3(SceneData[i].ViewMatrix));
			SceneData[i].LightDirection.x = 0.f;
			SceneData[i].LightDirection.y = -1.f;
			SceneData[i].LightDirection.z = 1.f;

			SceneData[i].AmbientIntensity = 1.f;
			SceneData[i].LighIntensity = 1.f;
			SceneData[i].LightColor = glm::vec3(1.f);
			SceneData[i].Exposure = 2.f;

			m_Device->UploadBufferData(SceneDataBuffer[i], &SceneData, sizeof(SceneDataStruct));
		}

		AddEventListener(Event::SystemEvent::CODE_RESIZED, &ResourceManager::ResizeEvent);
	}

	void ResourceManager::Shutdown()
	{
		RemoveEventListener(Event::SystemEvent::CODE_RESIZED, &ResourceManager::ResizeEvent);

		m_CurrentMeshIndex = 0;
		m_CurrentTextureIndex = 0;
		m_CurrentMaterialIndex = 0;
		m_CurrentRenderTargetIndex = 0;
		m_CurrentEnvironmentMapsIndex = 0;
		m_CurrentGraphicsPipelineIndex = 0;
		m_CurrentComputePipelineIndex = 0;

		SceneDataBuffer.clear();
		SceneData.clear();

		m_EnvironmentMaps.clear();
		m_Materials.clear();
		m_Textures.clear();
		m_Meshes.clear();
		m_RenderTargets.clear();
		m_GraphicsPipelines.clear();
		m_ComputePipelines.clear();
	}

	MeshHandle ResourceManager::CreateMesh(VertexBufferDesc vertexDesc, IndexBufferDesc indexDesc, void* vertexData, void* indexData)
	{
		MeshHandle handle = m_CurrentMeshIndex;

		Mesh mesh;

		mesh.VertexBuffer = m_Device->CreateVertexBuffer(vertexDesc);
		mesh.IndexBuffer = m_Device->CreateIndexBuffer(indexDesc);

		m_Device->UploadBufferData(mesh.VertexBuffer, vertexData, mesh.VertexBuffer.Desc.Stride *mesh.VertexBuffer.Desc.NumElements);
		m_Device->UploadBufferData(mesh.IndexBuffer, indexData, mesh.IndexBuffer.Desc.Stride*mesh.IndexBuffer.Desc.NumElements);

		m_Meshes[handle] = mesh;

		m_CurrentMeshIndex++;
		return handle;
	}

	TextureHandle ResourceManager::CreateTexture(TextureDesc textureDesc, void* imageData, bool mipMap)
	{
		TextureHandle handle = m_CurrentTextureIndex;
	
		Texture outTex = m_Device->CreateTexture(textureDesc);
		m_Textures[handle] = outTex;

		if (imageData != nullptr)
		{
			m_Device->UploadTextureData(m_Textures[handle], imageData, 0, 0);
		}
		if (mipMap)
		{
			MipMapTexture(m_Textures[handle]);
		}

		m_CurrentTextureIndex++;
		return handle;
	}

	MaterialHandle ResourceManager::CreateMaterial()
	{
		MaterialHandle handle = m_CurrentMaterialIndex;

		Material outMat;\

		ConstantBufferDesc MaterialBufferDesc =
		{
			sizeof(MaterialData)
		};
		MaterialBufferDesc.MemoryType = MemoryType::Shared;
		Gecko::Buffer materialConstantBuffer = m_Device->CreateConstantBuffer(MaterialBufferDesc);
		//MaterialData* materialData = reinterpret_cast<MaterialData*>(materialConstantBuffer._Buffer);
		MaterialData materialData = MaterialData();
		materialData.materialTextureFlags = 0;


		materialData.baseColorFactor[0] = static_cast<float>(1.f);
		materialData.baseColorFactor[1] = static_cast<float>(1.f);
		materialData.baseColorFactor[2] = static_cast<float>(1.f);
		materialData.baseColorFactor[3] = static_cast<float>(1.f);

		materialData.normalScale = static_cast<float>(0.f);

		materialData.matallicFactor = static_cast<float>(0.f);
		materialData.roughnessFactor = static_cast<float>(1.f);

		materialData.emissiveFactor[0] = static_cast<float>(0.f);
		materialData.emissiveFactor[1] = static_cast<float>(0.f);
		materialData.emissiveFactor[2] = static_cast<float>(0.f);

		materialData.occlusionStrength = static_cast<float>(1.f);

		materialData.materialTextureFlags = 0b00000;
		m_Device->UploadBufferData(materialConstantBuffer, &materialData, sizeof(MaterialData));

		outMat.AlbedoTextureHandle = GetMissingTextureHandle();
		outMat.EmmisiveTextureHandle = GetMissingTextureHandle();
		outMat.MetalicRoughnessTextureHandle = GetMissingTextureHandle();
		outMat.NormalTextureHandle = GetMissingTextureHandle();
		outMat.OcclusionTextureHandle = GetMissingTextureHandle();

		outMat.MaterialConstantBuffer = materialConstantBuffer;

		m_Materials[handle] = outMat;

		m_CurrentMaterialIndex++;
		return handle;
	}

	RenderTargetHandle ResourceManager::CreateRenderTarget(RenderTargetDesc renderTargetDesc, std::string name, bool KeepWindowAspectRatio)
	{
		RenderTargetHandle handle = m_CurrentRenderTargetIndex;

		RenderTarget outRenderTarget = m_Device->CreateRenderTarget(renderTargetDesc);
		m_RenderTargets[handle].RenderTarget = outRenderTarget;
		m_RenderTargets[handle].KeepWindowAspectRatio = KeepWindowAspectRatio;
		m_RenderTargets[handle].WidthScale = 1.f;

		m_CurrentRenderTargetIndex++;
		return handle;
	}

	EnvironmentMapHandle ResourceManager::CreateEnvironmentMap(std::string path)
	{
		RenderTargetHandle handle = m_CurrentEnvironmentMapsIndex;

		EnvironmentMap outEnvironmentMap;

		Texture HDRTexture;
		Texture EnvironmentTexture;
		Texture IrradianceTexture;

		// Make Cube map Texture
		{
			TextureDesc textureDesc;
			textureDesc.Width = 512;
			textureDesc.Height = 512;
			textureDesc.Type = TextureType::TexCube;
			textureDesc.Format = DataFormat::R32G32B32A32_FLOAT;
			textureDesc.NumMips = CalculateNumberOfMips(textureDesc.Width, textureDesc.Height);
			textureDesc.NumArraySlices = 6;
			outEnvironmentMap.EnvironmentTextureHandle = CreateTexture(textureDesc);
			EnvironmentTexture = GetTexture(outEnvironmentMap.EnvironmentTextureHandle);
		}

		// Load HDR Texture
		{
			int x, y, n;
			f32* data = stbi_loadf(Platform::GetLocalPath(path).c_str(), &x, &y, &n, 4);

			TextureDesc textureDesc;
			textureDesc.Width = x;
			textureDesc.Height = y;
			textureDesc.Type = TextureType::Tex2D;
			textureDesc.Format = DataFormat::R32G32B32A32_FLOAT;
			textureDesc.NumMips = 1;
			textureDesc.NumArraySlices = 1;
			outEnvironmentMap.HDRTextureHandle = CreateTexture(textureDesc, data);
			HDRTexture = GetTexture(outEnvironmentMap.HDRTextureHandle);;

			stbi_image_free(data);


			// computeShader to upload to cubemap

			{
				Ref<CommandList> commandList = m_Device->CreateComputeCommandList();

				commandList->BindComputePipeline(GetComputePipeline(HDRToCubeHandle));

				commandList->BindTexture(0, HDRTexture);

				commandList->BindAsRWTexture(0, EnvironmentTexture);

				commandList->Dispatch(
					EnvironmentTexture.Desc.Width / 32,
					EnvironmentTexture.Desc.Height / 32,
					6
				);

				m_Device->ExecuteComputeCommandList(commandList);
			}
			MipMapTexture(EnvironmentTexture);

		}

		// Generate IrradianceMap
		{
			TextureDesc textureDesc;
			textureDesc.Width = 32;
			textureDesc.Height = 32;
			textureDesc.Type = TextureType::TexCube;
			textureDesc.Format = DataFormat::R32G32B32A32_FLOAT;
			textureDesc.NumMips = CalculateNumberOfMips(textureDesc.Width, textureDesc.Height);
			textureDesc.NumArraySlices = 6;
			outEnvironmentMap.IrradianceTextureHandle = CreateTexture(textureDesc);
			IrradianceTexture = GetTexture(outEnvironmentMap.IrradianceTextureHandle);

			{
				Ref<CommandList> commandList = m_Device->CreateComputeCommandList();

				commandList->BindComputePipeline(GetComputePipeline(IrradianceMapHandle));

				commandList->BindTexture(0, EnvironmentTexture);
				commandList->BindAsRWTexture(0, IrradianceTexture);

				commandList->Dispatch(
					IrradianceTexture.Desc.Width / 32,
					IrradianceTexture.Desc.Height / 32,
					6
				);

				m_Device->ExecuteComputeCommandList(commandList);
			}
		}

		m_EnvironmentMaps[handle] = outEnvironmentMap;
		m_CurrentEnvironmentMapsIndex++;



		return handle;
	}

	GraphicsPipelineHandle ResourceManager::CreateGraphicsPipeline(GraphicsPipelineDesc graphicsPipelineDesc)
	{
		GraphicsPipelineHandle handle = m_CurrentGraphicsPipelineIndex;

		GraphicsPipeline outGraphicsPipeline = m_Device->CreateGraphicsPipeline(graphicsPipelineDesc);
		m_GraphicsPipelines[handle] = outGraphicsPipeline;

		m_CurrentGraphicsPipelineIndex++;
		return handle;
	}

	ComputePipelineHandle ResourceManager::CreateComputePipeline(ComputePipelineDesc computePipelineDesc)
	{
		ComputePipelineHandle handle = m_CurrentComputePipelineIndex;

		ComputePipeline outComputePipeline = m_Device->CreateComputePipeline(computePipelineDesc);
		m_ComputePipelines[handle] = outComputePipeline;

		m_CurrentComputePipelineIndex++;
		return handle;
	}

	Mesh& ResourceManager::GetMesh(const MeshHandle& meshHandle)
	{
		if (m_Meshes.find(meshHandle) == m_Meshes.end())
			return m_Meshes[m_CubeMeshHandle];

		return m_Meshes[meshHandle];
	}

	Texture& ResourceManager::GetTexture(const TextureHandle& textureHandle)
	{
		if (m_Textures.find(textureHandle) == m_Textures.end())
			return m_Textures[m_MissingTextureHandle];

		return m_Textures[textureHandle];
	}

	Material& ResourceManager::GetMaterial(const MaterialHandle& materialHandle)
	{
		if (m_Materials.find(materialHandle) == m_Materials.end())
			return m_Materials[m_MissingMaterialHandle];

		return m_Materials[materialHandle];
	}

	RenderTarget& ResourceManager::GetRenderTarget(const RenderTargetHandle& renderTargetHandle)
	{
		if (m_RenderTargets.find(renderTargetHandle) == m_RenderTargets.end())
		{
			ASSERT(false, "Could not find specified RenderTarget!");
		}

		return  m_RenderTargets[renderTargetHandle].RenderTarget;
	}

	EnvironmentMap& ResourceManager::GetEnvironmentMap(const EnvironmentMapHandle& environmentMapHandle)
	{
		return m_EnvironmentMaps[environmentMapHandle];
	}

	GraphicsPipeline& ResourceManager::GetGraphicsPipeline(const GraphicsPipelineHandle& graphicsPipelineHandle)
	{
		if (m_GraphicsPipelines.find(graphicsPipelineHandle) == m_GraphicsPipelines.end())
		{
			ASSERT(false, "Could not find specified GraphicsPipeline!");
		}

		return m_GraphicsPipelines[graphicsPipelineHandle];
	}

	ComputePipeline& ResourceManager::GetComputePipeline(const ComputePipelineHandle& computePipelineHandle)
	{
		if (m_ComputePipelines.find(computePipelineHandle) == m_ComputePipelines.end())
		{
			ASSERT(false, "Could not find specified ComputePipeline!");
		}

		return m_ComputePipelines[computePipelineHandle];
	}

	void ResourceManager::UploadMaterial(Buffer& buffer, void* data, u32 size, u32 offset)
	{
		m_Device->UploadBufferData(buffer, data, size, offset);
	}

	bool ResourceManager::ResizeEvent(const Event::EventData& eventData)
	{
		u32 width = eventData.Data.u32[0];
		u32 height = eventData.Data.u32[1];
		width = width == 0 ? 1 : width;
		height = height == 0 ? 1 : height;

		for (auto& [key, val] : m_RenderTargets)
		{
			if (val.KeepWindowAspectRatio)
			{
				RenderTargetDesc renderTargetDesc = val.RenderTarget.Desc;
				renderTargetDesc.Width = static_cast<u32>(width * val.WidthScale);
				renderTargetDesc.Height = static_cast<u32>(height * val.WidthScale);
				val.RenderTarget = m_Device->CreateRenderTarget(renderTargetDesc);
			}
		}

		return false;
	}

	void ResourceManager::MipMapTexture(Texture texture)
	{
		MipGenerationData mipGenerationData;

		mipGenerationData.Mip = 0;
		mipGenerationData.Srgb = texture.Desc.Format == DataFormat::R8G8B8A8_SRGB ? 1 : 0;
		mipGenerationData.Width = texture.Desc.Width;
		mipGenerationData.Height = texture.Desc.Height;

		while (mipGenerationData.Mip < texture.Desc.NumMips - 1)
		{

			Ref<CommandList> commandList = m_Device->CreateComputeCommandList();
			commandList->BindComputePipeline(GetComputePipeline(DownsamplePipelineHandle));
			commandList->SetLocalData(sizeof(MipGenerationData), &mipGenerationData);

			commandList->BindTexture(0, texture, mipGenerationData.Mip);

			commandList->BindAsRWTexture(0, texture, mipGenerationData.Mip + 1);

			commandList->Dispatch(
				std::max(1u, mipGenerationData.Width),
				std::max(1u, mipGenerationData.Height),
				std::max(1u, texture.Desc.NumArraySlices)
			);

			mipGenerationData.Mip += 1;
			mipGenerationData.Width = mipGenerationData.Width >> 1;
			mipGenerationData.Height = mipGenerationData.Height >> 1;
			m_Device->ExecuteComputeCommandList(commandList);
		}

	}

}