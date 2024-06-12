#include "Rendering/Frontend/Renderer/RenderPasses/FXAAPass.h"

#include "Rendering/Backend/CommandList.h"

#include "Rendering/Frontend/ResourceManager/ResourceManager.h"
#include "Rendering/Frontend/Scene/Scene.h"

namespace Gecko
{

const void FXAAPass::Init(ResourceManager* resourceManager)
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
		computePipelineDesc.ComputeShaderPath = "Shaders/FXAA";
		computePipelineDesc.DynamicCallData.BufferLocation = 0;
		computePipelineDesc.DynamicCallData.Size = sizeof(FXAAData);
		computePipelineDesc.SamplerDescs = computeSamplerShaderDescs;
		computePipelineDesc.NumTextures = 1;
		computePipelineDesc.NumUAVs = 1;


		FXAAPipelineHandle = resourceManager->CreateComputePipeline(computePipelineDesc);
	}

	Gecko::RenderTargetDesc renderTargetDesc;
	renderTargetDesc.AllowRenderTargetTexture = true;
	renderTargetDesc.Width = 1920;
	renderTargetDesc.Height = 1080;
	renderTargetDesc.NumRenderTargets = 1;
	for (u32 i = 0; i < renderTargetDesc.NumRenderTargets; i++)
	{
		renderTargetDesc.RenderTargetClearValues[i].Values[0] = 0.f;
		renderTargetDesc.RenderTargetClearValues[i].Values[1] = 0.f;
		renderTargetDesc.RenderTargetClearValues[i].Values[2] = 0.f;
		renderTargetDesc.RenderTargetClearValues[i].Values[3] = 0.f;
	}
	renderTargetDesc.RenderTargetFormats[0] = Gecko::Format::R32G32B32A32_FLOAT; // output

	m_OutputHandle = resourceManager->CreateRenderTarget(renderTargetDesc, "FXAAOutput");

	m_FXAAData.width = 1920;
	m_FXAAData.height = 1080;
	m_FXAAData.fxaaSpanMax = 8.f;
	m_FXAAData.fxaaReduceMul = 1.f / 128.f;
	m_FXAAData.fxaaReduceMin = 1. / 8.f;
}

const void FXAAPass::Render(const SceneDescriptor& sceneDescriptor, ResourceManager* resourceManager, Ref<CommandList> commandList)
{

	Ref<RenderTarget> inputTarget = resourceManager->GetRenderTarget(resourceManager->GetRenderTargetHandle("PBROutput"));
	Ref<RenderTarget> outputTarget = resourceManager->GetRenderTarget(m_OutputHandle);
	
	ComputePipeline FXAAPipeline = resourceManager->GetComputePipeline(FXAAPipelineHandle);

	commandList->BindComputePipeline(FXAAPipeline);

	commandList->SetDynamicCallData(sizeof(FXAAData), &m_FXAAData);
	commandList->BindTexture(0, inputTarget, Gecko::RenderTargetType::Target0);
	commandList->BindAsRWTexture(0, outputTarget, RenderTargetType::Target0);

	commandList->Dispatch(m_FXAAData.width / 8 + 1, m_FXAAData.height / 8 + 1, 1);
}

}