#include "Rendering/Frontend/Renderer/RenderPasses/FXAAPass.h"

#include "Rendering/Backend/CommandList.h"

#include "Rendering/Frontend/ResourceManager/ResourceManager.h"

namespace Gecko
{

const void FXAAPass::Init(Platform::AppInfo& appInfo, ResourceManager* resourceManager)
{
	// FXAA Compute Pipeline
	{
		std::vector<SamplerDesc> computeSamplerShaderDescs =
		{
			{
				ShaderVisibility::All,
				SamplerFilter::Point,
			}
		};

		ComputePipelineDesc computePipelineDesc;
		computePipelineDesc.ComputeShaderPath = "Shaders/FXAA.gsh";
		computePipelineDesc.ShaderVersion = "5_1";
		computePipelineDesc.DynamicCallData.BufferLocation = 0;
		computePipelineDesc.DynamicCallData.Size = sizeof(FXAAData);
		computePipelineDesc.SamplerDescs = computeSamplerShaderDescs;
		computePipelineDesc.NumTextures = 1;
		computePipelineDesc.NumUAVs = 1;


		FXAAPipelineHandle = resourceManager->CreateComputePipeline(computePipelineDesc);
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

	m_OutputHandle = resourceManager->CreateRenderTarget(renderTargetDesc, "FXAAOutput", true);

	m_FXAAData.width = appInfo.Width;
	m_FXAAData.height = appInfo.Height;
	m_FXAAData.fxaaSpanMax = 8.f;
	m_FXAAData.fxaaReduceMul = 1.f / 128.f;
	m_FXAAData.fxaaReduceMin = 1. / 8.f;

}

const void FXAAPass::Render(const SceneRenderInfo& sceneRenderInfo, ResourceManager* resourceManager, Ref<CommandList> commandList)
{

	RenderTarget inputTarget = resourceManager->GetRenderTarget(resourceManager->GetRenderTargetHandle("PBROutput"));
	RenderTarget outputTarget = resourceManager->GetRenderTarget(m_OutputHandle);
	
	ComputePipeline FXAAPipeline = resourceManager->GetComputePipeline(FXAAPipelineHandle);

	commandList->BindComputePipeline(FXAAPipeline);

	commandList->SetDynamicCallData(sizeof(FXAAData), &m_FXAAData);
	commandList->BindTexture(0, inputTarget, Gecko::RenderTargetType::Target0);
	commandList->BindAsRWTexture(0, outputTarget, RenderTargetType::Target0);

	commandList->Dispatch(m_FXAAData.width / 8 + 1, m_FXAAData.height / 8 + 1, 1);
}

}