#include "Rendering/Frontend/Renderer/RenderPasses/ToneMappingGammaCorrectionPass.h"

#include "Rendering/Backend/CommandList.h"

#include "Rendering/Frontend/ResourceManager/ResourceManager.h"

#include <algorithm>

namespace Gecko
{

const void ToneMappingGammaCorrectionPass::Init(Platform::AppInfo& appInfo, ResourceManager* resourceManager)
{
	// TonemapAndGammaCorrect Compute Pipeline
	{
		ComputePipelineDesc computePipelineDesc;
		computePipelineDesc.ComputeShaderPath = "Shaders/TonemapAndGammaCorrect";
		computePipelineDesc.NumUAVs = 2;
		computePipelineDesc.NumConstantBuffers = 1;

		TonemapAndGammaCorrectPipelineHandle = resourceManager->CreateComputePipeline(computePipelineDesc);
	}

	Gecko::RenderTargetDesc renderTargetDesc;
	renderTargetDesc.AllowRenderTargetTexture = true;
	renderTargetDesc.Width = appInfo.Width;
	renderTargetDesc.Height = appInfo.Height;
	renderTargetDesc.NumRenderTargets = 1;
	for (u32 i = 0; i < renderTargetDesc.NumRenderTargets; i++)
	{
		renderTargetDesc.RenderTargetClearValues[i].Values[0] = 0.f;
		renderTargetDesc.RenderTargetClearValues[i].Values[1] = 0.f;
		renderTargetDesc.RenderTargetClearValues[i].Values[2] = 0.f;
		renderTargetDesc.RenderTargetClearValues[i].Values[3] = 0.f;
	}
	renderTargetDesc.RenderTargetFormats[0] = Gecko::Format::R32G32B32A32_FLOAT; // output

	m_OutputHandle = resourceManager->CreateRenderTarget(renderTargetDesc, "ToneMappingGammaCorrection", true);
}

const void ToneMappingGammaCorrectionPass::Render(const SceneRenderInfo& sceneRenderInfo, ResourceManager* resourceManager, Ref<CommandList> commandList)
{
	RenderTarget inputTarget = resourceManager->GetRenderTarget(resourceManager->GetRenderTargetHandle("BloomOutput"));
	RenderTarget outputTarget = resourceManager->GetRenderTarget(m_OutputHandle);

	ComputePipeline TonemapAndGammaCorrectPipeline = resourceManager->GetComputePipeline(TonemapAndGammaCorrectPipelineHandle);

	commandList->BindComputePipeline(TonemapAndGammaCorrectPipeline);
	commandList->BindAsRWTexture(0, inputTarget, Gecko::RenderTargetType::Target0);
	commandList->BindAsRWTexture(1, outputTarget, Gecko::RenderTargetType::Target0);
	u32 currentBackBufferIndex = resourceManager->GetCurrentBackBufferIndex();
	commandList->BindConstantBuffer(0, resourceManager->SceneDataBuffer[currentBackBufferIndex]);
	commandList->Dispatch(
		std::max(1u, outputTarget.Desc.Width / 8 + 1),
		std::max(1u, outputTarget.Desc.Height / 8 + 1),
		1
	);
}


}