#include "Rendering/Frontend/Renderer/RenderPasses/FXAAPass.h"

#include "Rendering/Backend/CommandList.h"

#include "Rendering/Frontend/ResourceManager/ResourceManager.h"
#include "Rendering/Frontend/Renderer/Renderer.h"

namespace Gecko
{

const void FXAAPass::SubInit(const Platform::AppInfo& appInfo, ResourceManager* resourceManager, const ConfigData& dependencies)
{
	// FXAA Compute Pipeline
	{
		ComputePipelineDesc computePipelineDesc;
		computePipelineDesc.ComputeShaderPath = "Shaders/FXAA.gsh";
		computePipelineDesc.ShaderVersion = "5_1";
		computePipelineDesc.PipelineReadOnlyResources = 
		{ 
			PipelineResource::LocalData(ShaderType::Compute, 0, sizeof(FXAAData)),
			PipelineResource::Texture(ShaderType::Compute, 0) 
		};
		computePipelineDesc.SamplerDescs = {
			{ ShaderType::Compute, SamplerFilter::Point }
		};
		computePipelineDesc.PipelineReadWriteResources = {
			PipelineResource::Texture(ShaderType::Compute, 0)
		};


		FXAAPipelineHandle = resourceManager->CreateComputePipeline(computePipelineDesc);
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

	m_OutputHandle = resourceManager->CreateRenderTarget(renderTargetDesc, "FXAAOutput", true);

	m_FXAAData.width = appInfo.Width;
	m_FXAAData.height = appInfo.Height;
	m_FXAAData.fxaaSpanMax = 8.f;
	m_FXAAData.fxaaReduceMul = 1.f / 128.f;
	m_FXAAData.fxaaReduceMin = 1. / 8.f;

	m_ConfigData = dependencies;

}

const void FXAAPass::Render(const SceneRenderInfo& sceneRenderInfo, ResourceManager* resourceManager,
	const Renderer* renderer, Ref<CommandList> commandList)
{

	RenderTarget inputTarget = resourceManager->GetRenderTarget(renderer->GetRenderPassByHandle(m_ConfigData.PrevPass)->GetOutputHandle());
	RenderTarget outputTarget = resourceManager->GetRenderTarget(m_OutputHandle);
	
	ComputePipeline FXAAPipeline = resourceManager->GetComputePipeline(FXAAPipelineHandle);

	commandList->BindComputePipeline(FXAAPipeline);

	m_FXAAData.width = inputTarget.Desc.Width;
	m_FXAAData.height = inputTarget.Desc.Height;

	commandList->SetLocalData(sizeof(FXAAData), &m_FXAAData);
	commandList->BindTexture(0, inputTarget.RenderTextures[0]);
	commandList->BindAsRWTexture(0, outputTarget.RenderTextures[0]);

	commandList->Dispatch(m_FXAAData.width / 8 + 1, m_FXAAData.height / 8 + 1, 1);
}

}