#include "Rendering/Frontend/Renderer/RenderPasses/ToneMappingGammaCorrectionPass.h"

#include "Rendering/Backend/CommandList.h"

#include "Rendering/Frontend/ResourceManager/ResourceManager.h"
#include "Rendering/Frontend/Renderer/Renderer.h"

#include <algorithm>

namespace Gecko
{

const void ToneMappingGammaCorrectionPass::SubInit(const Platform::AppInfo& appInfo, ResourceManager* resourceManager,
	const ConfigData& dependencies)
{
	// TonemapAndGammaCorrect Compute Pipeline
	{
		ComputePipelineDesc computePipelineDesc;
		computePipelineDesc.ComputeShaderPath = "Shaders/TonemapAndGammaCorrect.gsh";
		computePipelineDesc.ShaderVersion = "5_1";
		computePipelineDesc.PipelineReadWriteResources = 
		{
			PipelineResource::Texture(ShaderVisibility::Compute, 0),
			PipelineResource::Texture(ShaderVisibility::Compute, 1)
		};
		computePipelineDesc.PipelineReadOnlyResources = 
		{
			PipelineResource::ConstantBuffer(ShaderVisibility::Compute, 0)
		};

		TonemapAndGammaCorrectPipelineHandle = resourceManager->CreateComputePipeline(computePipelineDesc);
	}

	Gecko::RenderTargetDesc renderTargetDesc;
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
	renderTargetDesc.RenderTargetFormats[0] = DataFormat::R32G32B32A32_FLOAT; // output

	m_OutputHandle = resourceManager->CreateRenderTarget(renderTargetDesc, "ToneMappingGammaCorrection", true);

	m_ConfigData = dependencies;
}

const void ToneMappingGammaCorrectionPass::Render(const SceneRenderInfo& sceneRenderInfo, ResourceManager* resourceManager,
	const Renderer* renderer, Ref<CommandList> commandList)
{
	RenderTarget inputTarget = resourceManager->GetRenderTarget(renderer->GetRenderPassByHandle(m_ConfigData.PrevPass)->GetOutputHandle());
	RenderTarget outputTarget = resourceManager->GetRenderTarget(m_OutputHandle);

	ComputePipeline TonemapAndGammaCorrectPipeline = resourceManager->GetComputePipeline(TonemapAndGammaCorrectPipelineHandle);

	commandList->BindComputePipeline(TonemapAndGammaCorrectPipeline);
	commandList->BindAsRWTexture(0, inputTarget.RenderTextures[0]);
	commandList->BindAsRWTexture(1, outputTarget.RenderTextures[0]);
	u32 currentBackBufferIndex = resourceManager->GetCurrentBackBufferIndex();
	commandList->BindConstantBuffer(0, resourceManager->SceneDataBuffer[currentBackBufferIndex]);
	commandList->Dispatch(
		std::max(1u, outputTarget.Desc.Width / 8 + 1),
		std::max(1u, outputTarget.Desc.Height / 8 + 1),
		1
	);
}


}