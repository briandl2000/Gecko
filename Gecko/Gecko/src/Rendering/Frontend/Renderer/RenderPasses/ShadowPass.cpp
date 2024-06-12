#include "Rendering/Frontend/Renderer/RenderPasses/ShadowPass.h"

#include "Rendering/Backend/CommandList.h"

#include "Rendering/Frontend/ResourceManager/ResourceManager.h"
#include "Rendering/Frontend/Scene/Scene.h"

namespace Gecko
{

const void ShadowPass::Init(ResourceManager* resourceManager)
{
	// Shadow map Graphics Pipeline
	{
		std::vector<ShaderVisibility> constantBufferVisibilities =
		{
			ShaderVisibility::Vertex,
		};

		GraphicsPipelineDesc pipelineDesc;
		pipelineDesc.VertexShaderPath = "Shaders/ShadowVertex";
		pipelineDesc.PixelShaderPath = "Shaders/ShadowPixel";

		pipelineDesc.VertexLayout = Vertex3D::GetLayout();

		pipelineDesc.ConstantBufferVisibilities = constantBufferVisibilities;

		pipelineDesc.RenderTargetFormats[0] = Format::R8G8B8A8_UNORM;
		pipelineDesc.DepthStencilFormat = Format::R32_FLOAT;

		pipelineDesc.WindingOrder = WindingOrder::CounterClockWise;
		pipelineDesc.CullMode = CullMode::Back;

		pipelineDesc.DynamicCallData.BufferLocation = 1;
		pipelineDesc.DynamicCallData.Size = sizeof(glm::mat4);
		pipelineDesc.DynamicCallData.ConstantBufferVisibilities = ShaderVisibility::Vertex;

		ShadowPipelineHandle = resourceManager->CreateGraphicsPipeline(pipelineDesc);

	}

	{
		Gecko::RenderTargetDesc ShadowMapTargetDesc;
		ShadowMapTargetDesc.AllowDepthStencilTexture = true;
		ShadowMapTargetDesc.RenderTargetFormats[0] = Gecko::Format::R8G8B8A8_UNORM; // Matallic Roughness Occlusion
		ShadowMapTargetDesc.NumRenderTargets = 1;
		ShadowMapTargetDesc.RenderTargetClearValues[0].Values[0] = 0.f;
		ShadowMapTargetDesc.RenderTargetClearValues[0].Values[1] = 0.f;
		ShadowMapTargetDesc.RenderTargetClearValues[0].Values[2] = 0.f;
		ShadowMapTargetDesc.RenderTargetClearValues[0].Values[3] = 0.f;
		ShadowMapTargetDesc.DepthStencilFormat = Gecko::Format::R32_FLOAT;
		ShadowMapTargetDesc.DepthTargetClearValue.Depth = 1.0f;
		ShadowMapTargetDesc.Width = 4096;
		ShadowMapTargetDesc.Height = 4096;
		m_OutputHandle = resourceManager->CreateRenderTarget(ShadowMapTargetDesc, "ShadowMap");
	}
}

const void ShadowPass::Render(const SceneDescriptor& sceneDescriptor, ResourceManager* resourceManager, Ref<CommandList> commandList)
{
	Ref<RenderTarget> outputTarget = resourceManager->GetRenderTarget(m_OutputHandle);
	GraphicsPipeline ShadowPipeline = resourceManager->GetGraphicsPipeline(ShadowPipelineHandle);

	commandList->ClearRenderTarget(outputTarget);

	commandList->BindGraphicsPipeline(ShadowPipeline);
	commandList->BindRenderTarget(outputTarget);
	commandList->BindConstantBuffer(0, resourceManager->SceneDataBuffer);

	for (u32 i = 0; i < sceneDescriptor.Meshes.size(); i++)
	{
		const MeshInstanceDescriptor& meshInstanceDescriptor = sceneDescriptor.Meshes[i];

		glm::mat4 transformMatrix = sceneDescriptor.NodeTransformMatrices[meshInstanceDescriptor.NodeTransformMatrixIndex];

		commandList->SetDynamicCallData(sizeof(glm::mat4), (void*)(&transformMatrix));

		Mesh& mesh = resourceManager->GetMesh(meshInstanceDescriptor.MeshInstance.MeshHandle);
		commandList->BindVertexBuffer(mesh.VertexBuffer);
		commandList->BindIndexBuffer(mesh.IndexBuffer);

		commandList->Draw(mesh.IndexBuffer->Desc.NumIndices);
	}
}

}