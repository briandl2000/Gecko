#include "Rendering/Frontend/Renderer/RenderPasses/ShadowRaytracePass.h"

#include "Rendering/Backend/CommandList.h"

#include "Rendering/Frontend/ResourceManager/ResourceManager.h"
#include "Rendering/Frontend/Renderer/Renderer.h"

namespace Gecko
{

const void ShadowRaytracePass::SubInit(const Platform::AppInfo& appInfo, ResourceManager* resourceManager,
	const ConfigData& dependencies)
{
	// Shadow map Graphics Pipeline
	{
		std::vector<ShaderVisibility> constantBufferVisibilities =
		{
			ShaderVisibility::Vertex,
		};

		RaytracingPipelineDesc pipelineDesc;
		pipelineDesc.RaytraceShaderPath = "Shaders/Raytracing.gsh";
		//pipelineDesc.ShaderVersion = "5_1";
		pipelineDesc.NumConstantBuffers = 1;
		pipelineDesc.NumUAVs = 3;

		m_ShadowRaytracePipelineHandle = resourceManager->CreateRaytracePipeline(pipelineDesc);

	}

	
	Gecko::RenderTargetDesc ShadowMapTargetDesc;
	ShadowMapTargetDesc.RenderTargetFormats[0] = DataFormat::R32_FLOAT;
	ShadowMapTargetDesc.NumRenderTargets = 1;
	ShadowMapTargetDesc.RenderTargetClearValues[0].Values[0] = 0.f;
	ShadowMapTargetDesc.RenderTargetClearValues[0].Values[1] = 0.f;
	ShadowMapTargetDesc.RenderTargetClearValues[0].Values[2] = 0.f;
	ShadowMapTargetDesc.RenderTargetClearValues[0].Values[3] = 0.f;
	ShadowMapTargetDesc.Width = appInfo.Width;
	ShadowMapTargetDesc.Height = appInfo.Height;
	m_OutputHandle = resourceManager->CreateRenderTarget(ShadowMapTargetDesc, "RaytracingShadowMap", true);

	m_ConfigData = dependencies;
}

const void ShadowRaytracePass::Render(const SceneRenderInfo& sceneRenderInfo, ResourceManager* resourceManager,
	const Renderer* renderer, Ref<CommandList> commandList)
{
	RenderTarget outputTarget = resourceManager->GetRenderTarget(m_OutputHandle);
	RenderTarget GBuffer = resourceManager->GetRenderTarget(renderer->GetRenderPassByHandle(m_ConfigData.GeoPass)->GetOutputHandle());


	RaytracingPipeline ShadowPipeline = resourceManager->GetRaytracingPipeline(m_ShadowRaytracePipelineHandle);

	commandList->ClearRenderTarget(outputTarget);

	commandList->BindRaytracingPipeline(ShadowPipeline);
	commandList->BindTLAS(*sceneRenderInfo.TLAS);
	u32 currentBackBufferIndex = resourceManager->GetCurrentBackBufferIndex();
	commandList->BindConstantBuffer(0, resourceManager->SceneDataBuffer[currentBackBufferIndex]);
	commandList->BindAsRWTexture(0, outputTarget.RenderTextures[0]);
	commandList->BindAsRWTexture(1, GBuffer.RenderTextures[2]);
	commandList->BindAsRWTexture(2, GBuffer.RenderTextures[1]);

	commandList->DispatchRays(1920, 1080, 1);

}

}