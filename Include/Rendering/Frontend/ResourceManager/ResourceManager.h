#pragma once

#include "Defines.h"
#include "Rendering/Frontend/ResourceManager/ResourceObjects.h"
#include "Rendering/Backend/Device.h"
#include "Core/Platform.h"
#include "Core/Event.h"

#include <unordered_map>

namespace Gecko
{

	class ResourceManager : protected Event::EventListener<ResourceManager>
	{
	public:

		ResourceManager() = default;
		~ResourceManager() {};

		void Init(Device* device);
		void Shutdown();

		MeshHandle CreateMesh(VertexBufferDesc vertexDesc, IndexBufferDesc indexDesc, void* vertexData, void* indexData);
		TextureHandle CreateTexture(TextureDesc textureDesc, void* imageData = nullptr, bool mipMap = false);
		RenderTargetHandle CreateRenderTarget(RenderTargetDesc renderTargetDesc, std::string name, bool KeepWindowAspectRatio);
		EnvironmentMapHandle CreateEnvironmentMap(std::string path);
		GraphicsPipelineHandle CreateGraphicsPipeline(GraphicsPipelineDesc graphicsPipelineDesc);
		ComputePipelineHandle CreateComputePipeline(ComputePipelineDesc computePipelineDesc);

		Mesh& GetMesh(const MeshHandle& meshHandle);
		Texture& GetTexture(const TextureHandle& textureHandle);
		RenderTarget& GetRenderTarget(const RenderTargetHandle& renderTargetHandle);
		EnvironmentMap& GetEnvironmentMap(const EnvironmentMapHandle& environmentMapHandle);
		GraphicsPipeline& GetGraphicsPipeline(const GraphicsPipelineHandle& graphicsPipelineHandle);
		ComputePipeline& GetComputePipeline(const ComputePipelineHandle& computePipelineHandle);

		TextureHandle GetMissingTextureHandle() { return m_MissingTextureHandle; }
		MeshHandle GetCubeMeshHandle() { return m_CubeMeshHandle; }

		u32 GetCurrentBackBufferIndex() { return m_Device->GetCurrentBackBufferIndex(); }

		//void UploadMaterial(Buffer& buffer, void* data, u32 size, u32 offset = 0);

		// TEMP
		std::vector<Buffer> SceneDataBuffer;
		std::vector<SceneDataStruct> SceneData;

		bool ResizeEvent(const Event::EventData& eventData);

		BufferHandle CreateVertexBuffer(const VertexBufferDesc& desc, void* vertexData = nullptr);
		BufferHandle CreateIndexBuffer(const IndexBufferDesc& desc, void* indexData = nullptr);
		BufferHandle CreateConstantBuffer(const ConstantBufferDesc& desc, void* constantData = nullptr);
		BufferHandle CreateStructuredBuffer(const StructuredBufferDesc& desc, void* structuredData = nullptr);

		Buffer& GetBuffer(const BufferHandle& bufferHandle);


		// TODO: REMOVE THIS this is just for testing
		BufferHandle GetMaterialBufferHandle() { return m_MaterialBufferHandle; };
		static constexpr u32 MAX_MATERIALS = 1024;
		u32 NewMaterialSlot() { static u32 materialSlot = 0; return materialSlot++; };
		void UploadMaterialData(MaterialData& data, u32 slot);

	private:
		void MipMapTexture(Texture texture);

	private:

		Device* m_Device;

		TextureHandle m_MissingTextureHandle{ 0 };
		MeshHandle m_CubeMeshHandle{ 0 };

		ComputePipelineHandle DownsamplePipelineHandle;
		ComputePipelineHandle HDRToCubeHandle;
		ComputePipelineHandle IrradianceMapHandle;

		u32 m_CurrentMeshIndex{ 0 };
		u32 m_CurrentTextureIndex{ 0 };
		u32 m_CurrentMaterialIndex{ 0 };
		u32 m_CurrentRenderTargetIndex{ 0 };
		u32 m_CurrentEnvironmentMapsIndex{ 0 };
		u32 m_CurrentGraphicsPipelineIndex{ 0 };
		u32 m_CurrentComputePipelineIndex{ 0 };
		u32 m_CurrentRaytracePipelineIndex{ 0 };
		u32 m_CurrentBufferIndex{ 0 };

		std::unordered_map<MeshHandle, Mesh> m_Meshes;
		std::unordered_map<TextureHandle, Texture> m_Textures;
		std::unordered_map<RenderTargetHandle, RenderTargetResource> m_RenderTargets;
		std::unordered_map<EnvironmentMapHandle, EnvironmentMap> m_EnvironmentMaps;

		std::unordered_map<GraphicsPipelineHandle, GraphicsPipeline> m_GraphicsPipelines;
		std::unordered_map<ComputePipelineHandle, ComputePipeline> m_ComputePipelines;

		std::unordered_map<BufferHandle, Buffer> m_Buffers;

		BufferHandle m_MaterialBufferHandle{ 0 };

	};

}