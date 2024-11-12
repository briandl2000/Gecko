#include "Rendering/Frontend/Renderer/RenderPasses/ShadowPass.h"

#include "Rendering/Backend/CommandList.h"

#include "Rendering/Frontend/ResourceManager/ResourceManager.h"

namespace Gecko
{

const void ShadowPass::SubInit(const Platform::AppInfo& appInfo, ResourceManager* resourceManager, const ConfigData& dependencies)
{
	// Shadow map Graphics Pipeline
	{
		std::vector<PipelineBuffer> pipelineBuffers =
		{
			PipelineBuffer::ConstantBuffer(ShaderVisibility::Vertex, 0),
			PipelineBuffer::LocalData(ShaderVisibility::Vertex, 1, sizeof(glm::mat4)),
		};

		GraphicsPipelineDesc pipelineDesc;
		pipelineDesc.VertexShaderPath = "Shaders/Shadow.gsh";
		pipelineDesc.ShaderVersion = "5_1";
		pipelineDesc.VertexLayout = Vertex3D::GetLayout();
		pipelineDesc.PipelineBuffers = pipelineBuffers;
		pipelineDesc.DepthStencilFormat = DataFormat::R32_FLOAT;
		pipelineDesc.WindingOrder = WindingOrder::CounterClockWise;
		pipelineDesc.CullMode = CullMode::Back;

		ShadowPipelineHandle = resourceManager->CreateGraphicsPipeline(pipelineDesc);
	}

	
	Gecko::RenderTargetDesc ShadowMapTargetDesc;
	ShadowMapTargetDesc.DepthStencilFormat = DataFormat::R32_FLOAT;
	ShadowMapTargetDesc.DepthTargetClearValue.Depth = 1.0f;
	ShadowMapTargetDesc.Width = 4096;
	ShadowMapTargetDesc.Height = 4096;
	m_OutputHandle = resourceManager->CreateRenderTarget(ShadowMapTargetDesc, "ShadowMap", false);

}

const void ShadowPass::Render(const SceneRenderInfo& sceneRenderInfo, ResourceManager* resourceManager,
	const Renderer* renderer, Ref<CommandList> commandList)
{
	RenderTarget outputTarget = resourceManager->GetRenderTarget(m_OutputHandle);
	GraphicsPipeline ShadowPipeline = resourceManager->GetGraphicsPipeline(ShadowPipelineHandle);

	commandList->ClearRenderTarget(outputTarget);

	commandList->BindGraphicsPipeline(ShadowPipeline);
	commandList->BindRenderTarget(outputTarget);
	u32 currentBackBufferIndex = resourceManager->GetCurrentBackBufferIndex();
	commandList->BindConstantBuffer(0, resourceManager->SceneDataBuffer[currentBackBufferIndex]);

	for (u32 i = 0; i < sceneRenderInfo.RenderObjects.size(); i++)
	{
		const RenderObjectRenderInfo& meshInstanceDescriptor = sceneRenderInfo.RenderObjects[i];

		glm::mat4 transformMatrix = meshInstanceDescriptor.Transform;

		commandList->SetLocalData(sizeof(glm::mat4), (void*)(&transformMatrix));

		Mesh& mesh = resourceManager->GetMesh(meshInstanceDescriptor.MeshHandle);
		commandList->BindVertexBuffer(mesh.VertexBuffer);
		commandList->BindIndexBuffer(mesh.IndexBuffer);

		commandList->Draw(mesh.IndexBuffer.Desc.NumIndices);
	}
}
}