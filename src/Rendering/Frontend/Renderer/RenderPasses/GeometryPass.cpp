#include "Rendering/Frontend/Renderer/RenderPasses/GeometryPass.h"

#include "Rendering/Backend/CommandList.h"

#include "Rendering/Frontend/ResourceManager/ResourceManager.h"

namespace Gecko
{

const void GeometryPass::SubInit(const Platform::AppInfo& appInfo, ResourceManager* resourceManager, const ConfigData& dependencies)
{
	// GBuffer Graphics Pipeline
	{	
		GraphicsPipelineDesc pipelineDesc;
		pipelineDesc.VertexShaderPath = "Shaders/GBufferShader.gsh";
		pipelineDesc.PixelShaderPath = "Shaders/GBufferShader.gsh";
		pipelineDesc.ShaderVersion = "5_1";

		pipelineDesc.VertexLayout = Vertex3D::GetLayout();

		pipelineDesc.PipelineResources = {
			PipelineResource::ConstantBuffer(ShaderVisibility::All, 0),
			PipelineResource::ConstantBuffer(ShaderVisibility::Pixel, 1),
			PipelineResource::LocalData(ShaderVisibility::All, 2, sizeof(glm::mat4)),
			PipelineResource::Texture(ShaderVisibility::Pixel, 0),
			PipelineResource::Texture(ShaderVisibility::Pixel, 1),
			PipelineResource::Texture(ShaderVisibility::Pixel, 2),
			PipelineResource::Texture(ShaderVisibility::Pixel, 3),
			PipelineResource::Texture(ShaderVisibility::Pixel, 4),
		};

		pipelineDesc.RenderTextureFormats[0] = DataFormat::R32G32B32A32_FLOAT; // Albedo
		pipelineDesc.RenderTextureFormats[1] = DataFormat::R32G32B32A32_FLOAT; // Normal
		pipelineDesc.RenderTextureFormats[2] = DataFormat::R32G32B32A32_FLOAT; // Position
		pipelineDesc.RenderTextureFormats[3] = DataFormat::R32G32B32A32_FLOAT; // Emissive
		pipelineDesc.RenderTextureFormats[4] = DataFormat::R32G32B32A32_FLOAT; // Matallic Roughness Occlusion
		pipelineDesc.DepthStencilFormat = DataFormat::R32_FLOAT;

		pipelineDesc.WindingOrder = WindingOrder::CounterClockWise;
		pipelineDesc.CullMode = CullMode::Back;
		pipelineDesc.PrimitiveType = PrimitiveType::Triangles;

		pipelineDesc.SamplerDescs = {
			{ShaderVisibility::Pixel, SamplerFilter::Linear},
			{ShaderVisibility::Pixel, SamplerFilter::Point}
		};

		GBufferPipelineHandle = resourceManager->CreateGraphicsPipeline(pipelineDesc);
	}

	// CubeMap Graphics Pipeline
	{

		GraphicsPipelineDesc pipelineDesc;
		pipelineDesc.VertexShaderPath = "Shaders/Cubemap.gsh";
		pipelineDesc.PixelShaderPath = "Shaders/Cubemap.gsh";
		pipelineDesc.ShaderVersion = "5_1";
		pipelineDesc.VertexLayout = Vertex3D::GetLayout();

		pipelineDesc.PipelineResources = 
		{	
			PipelineResource::ConstantBuffer(ShaderVisibility::All, 0),
			PipelineResource::Texture(ShaderVisibility::Pixel, 0)
		};
		pipelineDesc.RenderTextureFormats[0] = DataFormat::R32G32B32A32_FLOAT; // Albedo
		pipelineDesc.RenderTextureFormats[1] = DataFormat::R32G32B32A32_FLOAT; // Normal
		pipelineDesc.RenderTextureFormats[2] = DataFormat::R32G32B32A32_FLOAT; // Position
		pipelineDesc.RenderTextureFormats[3] = DataFormat::R32G32B32A32_FLOAT; // Emissive
		pipelineDesc.RenderTextureFormats[4] = DataFormat::R32G32B32A32_FLOAT; // Matallic Roughness Occlusion
		pipelineDesc.DepthStencilFormat = DataFormat::R32_FLOAT;
		pipelineDesc.CullMode = CullMode::Back;

		pipelineDesc.SamplerDescs = {
			{ ShaderVisibility::Pixel, SamplerFilter::Linear,},
		};

		CubemapPipelineHandle = resourceManager->CreateGraphicsPipeline(pipelineDesc);
	}
	

	RenderTargetDesc gbufferTargetDesc;
	gbufferTargetDesc.DepthStencilFormat = DataFormat::R32_FLOAT;
	gbufferTargetDesc.DepthTargetClearValue.Depth = 1.0f;
	gbufferTargetDesc.Width = appInfo.Width;
	gbufferTargetDesc.Height = appInfo.Height;
	gbufferTargetDesc.NumRenderTargets = 5;
	for (u32 i = 0; i < gbufferTargetDesc.NumRenderTargets; i++)
	{
		gbufferTargetDesc.RenderTargetClearValues[i].Values[0] = 0.f;
		gbufferTargetDesc.RenderTargetClearValues[i].Values[1] = 0.f;
		gbufferTargetDesc.RenderTargetClearValues[i].Values[2] = 0.f;
		gbufferTargetDesc.RenderTargetClearValues[i].Values[3] = 0.f;
	}
	gbufferTargetDesc.RenderTargetFormats[0] = DataFormat::R32G32B32A32_FLOAT; // Albedo
	gbufferTargetDesc.RenderTargetFormats[1] = DataFormat::R32G32B32A32_FLOAT; // Normal
	gbufferTargetDesc.RenderTargetFormats[2] = DataFormat::R32G32B32A32_FLOAT; // Position
	gbufferTargetDesc.RenderTargetFormats[3] = DataFormat::R32G32B32A32_FLOAT; // Emissive
	gbufferTargetDesc.RenderTargetFormats[4] = DataFormat::R32G32B32A32_FLOAT; // Matallic Roughness Occlusion

	m_OutputHandle = resourceManager->CreateRenderTarget(gbufferTargetDesc, "GBuffer", true);
}

const void GeometryPass::Render(const SceneRenderInfo& sceneRenderInfo, ResourceManager* resourceManager,
	const Renderer* renderer, Ref<CommandList> commandList)
{
	RenderTarget OutputTarget = resourceManager->GetRenderTarget(m_OutputHandle);

	GraphicsPipeline CubemapPipeline = resourceManager->GetGraphicsPipeline(CubemapPipelineHandle);
	GraphicsPipeline GBufferPipeline = resourceManager->GetGraphicsPipeline(GBufferPipelineHandle);

	// Environment map pass
	commandList->ClearRenderTarget(OutputTarget);
	commandList->BindRenderTarget(OutputTarget);

	commandList->BindGraphicsPipeline(CubemapPipeline);
	u32 currentBackBufferIndex = resourceManager->GetCurrentBackBufferIndex();
	commandList->BindConstantBuffer(0, resourceManager->SceneDataBuffer[currentBackBufferIndex]);
	Mesh& cubeMesh = resourceManager->GetMesh(resourceManager->GetCubeMeshHandle());
	commandList->BindVertexBuffer(cubeMesh.VertexBuffer);
	commandList->BindIndexBuffer(cubeMesh.IndexBuffer);
	EnvironmentMap environmentMap = resourceManager->GetEnvironmentMap(sceneRenderInfo.EnvironmentMap);
	commandList->BindTexture(0, resourceManager->GetTexture(environmentMap.EnvironmentTextureHandle));

	commandList->Draw(cubeMesh.IndexBuffer.Desc.NumElements);

	// Geometry pass

	commandList->BindGraphicsPipeline(GBufferPipeline);
	commandList->BindConstantBuffer(0, resourceManager->SceneDataBuffer[currentBackBufferIndex]);
	for (u32 i = 0; i < sceneRenderInfo.RenderObjects.size(); i++)
	{
		const RenderObjectRenderInfo& meshInstanceDescriptor = sceneRenderInfo.RenderObjects[i];

		glm::mat4 transformMatrix = meshInstanceDescriptor.Transform;

		commandList->SetLocalData(sizeof(glm::mat4), (void*)(&transformMatrix));

		Mesh& mesh = resourceManager->GetMesh(meshInstanceDescriptor.MeshHandle);
		commandList->BindVertexBuffer(mesh.VertexBuffer);
		commandList->BindIndexBuffer(mesh.IndexBuffer);

		Material& material = resourceManager->GetMaterial(meshInstanceDescriptor.MaterialHandle);
		commandList->BindTexture(0, resourceManager->GetTexture(material.AlbedoTextureHandle));
		commandList->BindTexture(1, resourceManager->GetTexture(material.NormalTextureHandle));
		commandList->BindTexture(2, resourceManager->GetTexture(material.MetalicRoughnessTextureHandle));
		commandList->BindTexture(3, resourceManager->GetTexture(material.EmmisiveTextureHandle));
		commandList->BindTexture(4, resourceManager->GetTexture(material.OcclusionTextureHandle));
		commandList->BindConstantBuffer(1, material.MaterialConstantBuffer);

		commandList->Draw(mesh.IndexBuffer.Desc.NumElements);
	}
}


}