#include "Rendering/Frontend/Renderer/RenderPasses/ShadowRaytracePass.h"

#include "Rendering/Backend/CommandList.h"

#include "Rendering/Frontend/ResourceManager/ResourceManager.h"
#include "Rendering/Frontend/Scene/Scene.h"

namespace Gecko
{

const void ShadowRaytracePass::Init(ResourceManager* resourceManager)
{
	// Shadow map Graphics Pipeline
	{
		std::vector<ShaderVisibility> constantBufferVisibilities =
		{
			ShaderVisibility::Vertex,
		};

		RaytracingPipelineDesc pipelineDesc;
		pipelineDesc.NumConstantBuffers = 1;
		pipelineDesc.NumUAVs = 3;

		m_ShadowRaytracePipelineHandle = resourceManager->CreateRaytracePipeline(pipelineDesc);

	}

	{
		Gecko::RenderTargetDesc ShadowMapTargetDesc;
		ShadowMapTargetDesc.AllowRenderTargetTexture = true;
		ShadowMapTargetDesc.RenderTargetFormats[0] = Gecko::Format::R32_FLOAT;
		ShadowMapTargetDesc.NumRenderTargets = 1;
		ShadowMapTargetDesc.RenderTargetClearValues[0].Values[0] = 0.f;
		ShadowMapTargetDesc.RenderTargetClearValues[0].Values[1] = 0.f;
		ShadowMapTargetDesc.RenderTargetClearValues[0].Values[2] = 0.f;
		ShadowMapTargetDesc.RenderTargetClearValues[0].Values[3] = 0.f;
		ShadowMapTargetDesc.Width = 1920;
		ShadowMapTargetDesc.Height = 1080;
		m_OutputHandle = resourceManager->CreateRenderTarget(ShadowMapTargetDesc, "RaytracingShadowMap");
	}
}

const void ShadowRaytracePass::Render(const SceneDescriptor& sceneDescriptor, ResourceManager* resourceManager, Ref<CommandList> commandList)
{
	Ref<RenderTarget> outputTarget = resourceManager->GetRenderTarget(m_OutputHandle);
	Ref<RenderTarget> GBuffer = resourceManager->GetRenderTarget(resourceManager->GetRenderTargetHandle("GBuffer"));


	RaytracingPipeline ShadowPipeline = resourceManager->GetRaytracingPipeline(m_ShadowRaytracePipelineHandle);

	commandList->ClearRenderTarget(outputTarget);

	commandList->BindRaytracingPipeline(ShadowPipeline);
	commandList->BindTLAS(*sceneDescriptor.TLAS);
	commandList->BindConstantBuffer(0, resourceManager->SceneDataBuffer);
	commandList->BindAsRWTexture(0, outputTarget, RenderTargetType::Target0);
	commandList->BindAsRWTexture(1, GBuffer, RenderTargetType::Target2);
	commandList->BindAsRWTexture(2, GBuffer, RenderTargetType::Target1);

	commandList->DispatchRays(1920, 1080, 1);

}

}