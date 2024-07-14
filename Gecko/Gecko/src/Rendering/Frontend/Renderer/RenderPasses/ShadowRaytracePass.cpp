#include "Rendering/Frontend/Renderer/RenderPasses/ShadowRaytracePass.h"

#include "Rendering/Backend/CommandList.h"

#include "Rendering/Frontend/ResourceManager/ResourceManager.h"

namespace Gecko
{

const void ShadowRaytracePass::Init(Platform::AppInfo& appInfo, ResourceManager* resourceManager)
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

	
	Gecko::RenderTargetDesc ShadowMapTargetDesc;
	ShadowMapTargetDesc.AllowRenderTargetTexture = true;
	ShadowMapTargetDesc.RenderTargetFormats[0] = Gecko::Format::R32_FLOAT;
	ShadowMapTargetDesc.NumRenderTargets = 1;
	ShadowMapTargetDesc.RenderTargetClearValues[0].Values[0] = 0.f;
	ShadowMapTargetDesc.RenderTargetClearValues[0].Values[1] = 0.f;
	ShadowMapTargetDesc.RenderTargetClearValues[0].Values[2] = 0.f;
	ShadowMapTargetDesc.RenderTargetClearValues[0].Values[3] = 0.f;
	ShadowMapTargetDesc.Width = appInfo.Width;
	ShadowMapTargetDesc.Height = appInfo.Height;
	m_OutputHandle = resourceManager->CreateRenderTarget(ShadowMapTargetDesc, "RaytracingShadowMap", true);


}

const void ShadowRaytracePass::Render(const SceneRenderInfo& sceneRenderInfo, ResourceManager* resourceManager, Ref<CommandList> commandList)
{
	Ref<RenderTarget> outputTarget = resourceManager->GetRenderTarget(m_OutputHandle);
	Ref<RenderTarget> GBuffer = resourceManager->GetRenderTarget(resourceManager->GetRenderTargetHandle("GBuffer"));


	RaytracingPipeline ShadowPipeline = resourceManager->GetRaytracingPipeline(m_ShadowRaytracePipelineHandle);

	commandList->ClearRenderTarget(outputTarget);

	commandList->BindRaytracingPipeline(ShadowPipeline);
	commandList->BindTLAS(*sceneRenderInfo.TLAS);
	u32 currentBackBufferIndex = resourceManager->GetCurrentBackBufferIndex();
	commandList->BindConstantBuffer(0, resourceManager->SceneDataBuffer[currentBackBufferIndex]);
	commandList->BindAsRWTexture(0, outputTarget, RenderTargetType::Target0);
	commandList->BindAsRWTexture(1, GBuffer, RenderTargetType::Target2);
	commandList->BindAsRWTexture(2, GBuffer, RenderTargetType::Target1);

	commandList->DispatchRays(1920, 1080, 1);

}

}