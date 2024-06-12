#pragma once

#include "Defines.h"
#include "Rendering/Frontend/ResourceManager/ResourceObjects.h"
#include "Rendering/Backend/Device.h"
#include "Platform/Platform.h"

#include <unordered_map>

namespace Gecko
{

class ResourceManager
{
public:

	ResourceManager() = default;
	~ResourceManager() {};

	void Init(Device* device);
	void Shutdown();

	MeshHandle CreateMesh(VertexBufferDesc vertexDesc, IndexBufferDesc indexDesc, bool CreateBLAS);
	TextureHandle CreateTexture(TextureDesc textureDesc, void* imageData = nullptr, bool mipMap = false);
	MaterialHandle CreateMaterial();
	RenderTargetHandle CreateRenderTarget(RenderTargetDesc renderTargetDesc, std::string name);
	EnvironmentMapHandle CreateEnvironmentMap(std::string path);
	GraphicsPipelineHandle CreateGraphicsPipeline(GraphicsPipelineDesc graphicsPipelineDesc);
	ComputePipelineHandle CreateComputePipeline(ComputePipelineDesc computePipelineDesc);
	RaytracingPipelineHandle CreateRaytracePipeline(RaytracingPipelineDesc raytracePipelineDesc);

	RenderTargetHandle GetRenderTargetHandle(std::string name);

	Mesh& GetMesh(const MeshHandle& meshHandle);
	Ref<Texture>& GetTexture(const TextureHandle& textureHandle);
	Material& GetMaterial(const MaterialHandle& materialHandle);
	Ref<RenderTarget>& GetRenderTarget(const RenderTargetHandle& renderTargetHandle);
	EnvironmentMap& GetEnvironmentMap(const EnvironmentMapHandle& environmentMapHandle);
	GraphicsPipeline& GetGraphicsPipeline(const GraphicsPipelineHandle& graphicsPipelineHandle);
	ComputePipeline& GetComputePipeline(const ComputePipelineHandle& computePipelineHandle);
	RaytracingPipeline& GetRaytracingPipeline(const RaytracingPipelineHandle& raytracingPipelineHandle);

	TextureHandle GetMissingTextureHandle() { return m_MissingTextureHandle; }
	MaterialHandle GetMissingMaterialHandle() { return m_MissingMaterialHandle; }
	MaterialHandle GetCubeMeshHandle() { return m_CubeMeshHandle; }

	// TEMP
	Gecko::Ref<Gecko::ConstantBuffer> SceneDataBuffer;
	SceneDataStruct* SceneData;

private:
	void MipMapTexture(Ref<Texture> texture);

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
	std::unordered_map<TextureHandle, Ref<Texture>> m_Textures;
	std::unordered_map<MaterialHandle, Material> m_Materials;
	std::unordered_map<RenderTargetHandle, Ref<RenderTarget>> m_RenderTargets;
	std::unordered_map<EnvironmentMapHandle, EnvironmentMap> m_EnvironmentMaps;

	std::unordered_map<GraphicsPipelineHandle, GraphicsPipeline> m_GraphicsPipelines;
	std::unordered_map<ComputePipelineHandle, ComputePipeline> m_ComputePipelines;
	std::unordered_map<RaytracingPipelineHandle, RaytracingPipeline> m_RaytracePipelines;

	std::unordered_map<std::string, RenderTargetHandle> m_RenderTargateHandles;

};

}