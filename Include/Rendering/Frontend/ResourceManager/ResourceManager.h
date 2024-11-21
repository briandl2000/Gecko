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
	//GECKO_ADD_EVENT_LISTENERS(ResourceManager)
public:

	ResourceManager() = default;
	~ResourceManager() {};

	void Init(Device* device);
	void Shutdown();

	MeshHandle CreateMesh(VertexBufferDesc vertexDesc, IndexBufferDesc indexDesc, void* vertexData, void* indexData);
	TextureHandle CreateTexture(TextureDesc textureDesc, void* imageData = nullptr, bool mipMap = false);
	MaterialHandle CreateMaterial();
	RenderTargetHandle CreateRenderTarget(RenderTargetDesc renderTargetDesc, std::string name, bool KeepWindowAspectRatio);
	EnvironmentMapHandle CreateEnvironmentMap(std::string path);
	GraphicsPipelineHandle CreateGraphicsPipeline(GraphicsPipelineDesc graphicsPipelineDesc);
	ComputePipelineHandle CreateComputePipeline(ComputePipelineDesc computePipelineDesc);

	Mesh& GetMesh(const MeshHandle& meshHandle);
	Texture& GetTexture(const TextureHandle& textureHandle);
	Material& GetMaterial(const MaterialHandle& materialHandle);
	RenderTarget& GetRenderTarget(const RenderTargetHandle& renderTargetHandle);
	EnvironmentMap& GetEnvironmentMap(const EnvironmentMapHandle& environmentMapHandle);
	GraphicsPipeline& GetGraphicsPipeline(const GraphicsPipelineHandle& graphicsPipelineHandle);
	ComputePipeline& GetComputePipeline(const ComputePipelineHandle& computePipelineHandle);

	TextureHandle GetMissingTextureHandle() { return m_MissingTextureHandle; }
	MaterialHandle GetMissingMaterialHandle() { return m_MissingMaterialHandle; }
	MaterialHandle GetCubeMeshHandle() { return m_CubeMeshHandle; }

	u32 GetCurrentBackBufferIndex() { return m_Device->GetCurrentBackBufferIndex(); }

	void UploadMaterial(Buffer& buffer, void* data, u32 size, u32 offset = 0);

	// TEMP
	std::vector<Buffer> SceneDataBuffer;
	std::vector<SceneDataStruct> SceneData;

	bool ResizeEvent(const Event::EventData& eventData);

private:
	void MipMapTexture(Texture texture);

private:

	Device* m_Device;

	TextureHandle m_MissingTextureHandle{ 0 };
	MaterialHandle m_MissingMaterialHandle{ 0 };
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

	std::unordered_map<MeshHandle, Mesh> m_Meshes;
	std::unordered_map<TextureHandle, Texture> m_Textures;
	std::unordered_map<MaterialHandle, Material> m_Materials;
	std::unordered_map<RenderTargetHandle, RenderTargetResource> m_RenderTargets;
	std::unordered_map<EnvironmentMapHandle, EnvironmentMap> m_EnvironmentMaps;

	std::unordered_map<GraphicsPipelineHandle, GraphicsPipeline> m_GraphicsPipelines;
	std::unordered_map<ComputePipelineHandle, ComputePipeline> m_ComputePipelines;

};

}