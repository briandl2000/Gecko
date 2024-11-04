#include "CustomPass.h"

#include "Defines.h"
#include "Rendering/Backend/CommandList.h"
#include "Rendering/Frontend/Renderer/Renderer.h"

const void CustomPass::SubInit(const Gecko::Platform::AppInfo& appInfo, Gecko::ResourceManager* resourceManager,
	const ConfigData& dependencies)
{
	// Simple colour output Compute Pipeline
	{
		Gecko::ComputePipelineDesc computePipelineDesc;
		computePipelineDesc.ComputeShaderPath = "Shaders/Custom.gsh";
		computePipelineDesc.ShaderVersion = "5_1";
		computePipelineDesc.NumUAVs = 1;

		CustomPipelineHandle = resourceManager->CreateComputePipeline(computePipelineDesc);
	}

	Gecko::RenderTargetDesc renderTargetDesc;
	renderTargetDesc.AllowRenderTargetTexture = true;
	renderTargetDesc.Width = appInfo.Width;
	renderTargetDesc.Height = appInfo.Height;
	renderTargetDesc.NumRenderTargets = 1;
	for (Gecko::u32 i = 0; i < renderTargetDesc.NumRenderTargets; i++)
	{
		renderTargetDesc.RenderTargetClearValues[i].Values[0] = 0.f;
		renderTargetDesc.RenderTargetClearValues[i].Values[1] = 0.f;
		renderTargetDesc.RenderTargetClearValues[i].Values[2] = 0.f;
		renderTargetDesc.RenderTargetClearValues[i].Values[3] = 0.f;
	}
	renderTargetDesc.RenderTargetFormats[0] = Gecko::Format::R32G32B32A32_FLOAT; // output

	m_OutputHandle = resourceManager->CreateRenderTarget(renderTargetDesc, "ToneMappingGammaCorrection", true);
}

const void CustomPass::Render(const Gecko::SceneRenderInfo& sceneRenderInfo, Gecko::ResourceManager* resourceManager,
	const Gecko::Renderer* renderer, Gecko::Ref<Gecko::CommandList> commandList)
{
	Gecko::RenderTarget outputTarget = resourceManager->GetRenderTarget(m_OutputHandle);

	Gecko::ComputePipeline CustomPipeline = resourceManager->GetComputePipeline(CustomPipelineHandle);

	commandList->BindComputePipeline(CustomPipeline);
	commandList->BindAsRWTexture(0, outputTarget.RenderTextures[0]);
	commandList->Dispatch(
		std::max(1u, outputTarget.Desc.Width / 8 + 1),
		std::max(1u, outputTarget.Desc.Height / 8 + 1),
		1
	);
}
