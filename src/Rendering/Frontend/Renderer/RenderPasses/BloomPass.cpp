#include "Rendering/Frontend/Renderer/RenderPasses/BloomPass.h"

#include "Rendering/Backend/CommandList.h"

#include "Rendering/Frontend/ResourceManager/ResourceManager.h"
#include "Rendering/Frontend/Renderer/Renderer.h"

#include <algorithm>

namespace Gecko
{

const void BloomPass::SubInit(const Platform::AppInfo& appInfo, ResourceManager* resourceManager, const ConfigData& dependencies)
{
	// BloomDownScale Compute Pipeline
	{
		ComputePipelineDesc computePipelineDesc;
		computePipelineDesc.ComputeShaderPath = "Shaders/Bloom/BloomDownScale.gsh";
		computePipelineDesc.ShaderVersion = "5_1";
		computePipelineDesc.PipelineReadOnlyResources = {
			PipelineResource::LocalData(ShaderType::Compute, 0, sizeof(BloomData)),
			PipelineResource::Texture(ShaderType::Compute, 0)
		};
		computePipelineDesc.PipelineReadWriteResources = {
			PipelineResource::Texture(ShaderType::Compute, 0),
			PipelineResource::Texture(ShaderType::Compute, 1)
		};
		computePipelineDesc.SamplerDescs = {
			{ ShaderType::Compute, SamplerFilter::Linear, SamplerWrapMode::Clamp, SamplerWrapMode::Clamp, SamplerWrapMode::Clamp }
		};

		m_DownScalePipelineHandle = resourceManager->CreateComputePipeline(computePipelineDesc);
	}

	// BloomUpScale Compute Pipeline
	{
		ComputePipelineDesc computePipelineDesc;
		computePipelineDesc.ComputeShaderPath = "Shaders/Bloom/BloomUpScale.gsh";
		computePipelineDesc.ShaderVersion = "5_1";
		computePipelineDesc.PipelineReadOnlyResources = {
			PipelineResource::LocalData(ShaderType::Compute, 0, sizeof(BloomData)),
			PipelineResource::Texture(ShaderType::Compute, 0)
		};
		computePipelineDesc.PipelineReadWriteResources = {
			PipelineResource::Texture(ShaderType::Compute, 0),
			PipelineResource::Texture(ShaderType::Compute, 1)
		};
		computePipelineDesc.SamplerDescs = {
			{ShaderType::Compute, SamplerFilter::Linear, SamplerWrapMode::Clamp, SamplerWrapMode::Clamp, SamplerWrapMode::Clamp,}
		};
		m_UpScalePipelineHandle = resourceManager->CreateComputePipeline(computePipelineDesc);

	}

	// BloomThreshold Compute Pipeline
	{

		ComputePipelineDesc computePipelineDesc;
		computePipelineDesc.ComputeShaderPath = "Shaders/Bloom/BloomThreshold.gsh";
		computePipelineDesc.ShaderVersion = "5_1";
		computePipelineDesc.PipelineReadOnlyResources = 
		{ 
			PipelineResource::LocalData(ShaderType::Compute, 0, sizeof(BloomData)) 
		};
		computePipelineDesc.PipelineReadWriteResources = {
			PipelineResource::Texture(ShaderType::Compute, 0),
		};

		m_ThresholdPipelineHandle = resourceManager->CreateComputePipeline(computePipelineDesc);

	}

	// BloomComposite Compute Pipeline
	{
		ComputePipelineDesc computePipelineDesc;
		computePipelineDesc.ComputeShaderPath = "Shaders/Bloom/BloomComposite.gsh";
		computePipelineDesc.ShaderVersion = "5_1";
		computePipelineDesc.PipelineReadWriteResources = {
			PipelineResource::Texture(ShaderType::Compute, 0),
			PipelineResource::Texture(ShaderType::Compute, 1)
		};

		m_CompositePipelineHandle = resourceManager->CreateComputePipeline(computePipelineDesc);
	}

	RenderTargetDesc renderTargetDesc;

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
	renderTargetDesc.RenderTargetFormats[0] = DataFormat::R32G32B32A32_FLOAT;
	renderTargetDesc.NumMips[0] = 8;

	m_DownScaleRenderTargetHandle = resourceManager->CreateRenderTarget(renderTargetDesc, "m_DownScaleRenderTargetHandle", true);
	m_UpScaleRenderTargetHandle = resourceManager->CreateRenderTarget(renderTargetDesc, "m_UpScaleRenderTargetHandle", true);

	m_BloomData.Width = appInfo.Width;
	m_BloomData.Height = appInfo.Height;
	m_BloomData.Threshold = .9f;
	
	// This pass modifies the render target output of the previous pass in-place,
	// so no need to create a new output target for the final output
	m_OutputHandle = dependencies.PrevPassOutput;
}

const void BloomPass::Render(const SceneRenderInfo& sceneRenderInfo, ResourceManager* resourceManager,
	const Renderer* renderer, Ref<CommandList> commandList)
{
	RenderTarget downSampleTexture = resourceManager->GetRenderTarget(m_DownScaleRenderTargetHandle);
	RenderTarget upSampleTexture = resourceManager->GetRenderTarget(m_UpScaleRenderTargetHandle);
	RenderTarget modifyTarget = resourceManager->GetRenderTarget(m_OutputHandle);

	ComputePipeline BloomDownScale = resourceManager->GetComputePipeline(m_DownScalePipelineHandle);
	ComputePipeline BloomUpScale = resourceManager->GetComputePipeline(m_UpScalePipelineHandle);
	ComputePipeline BloomThreshold = resourceManager->GetComputePipeline(m_ThresholdPipelineHandle);
	ComputePipeline BloomComposite = resourceManager->GetComputePipeline(m_CompositePipelineHandle);

	commandList->CopyTextureToTexture(
		modifyTarget.RenderTextures[0],
		downSampleTexture.RenderTextures[0]
	);

	u32 mipLevel = 0;
	BloomData tempBloomData = m_BloomData;
	tempBloomData.Width = downSampleTexture.Desc.Width >> 1;
	tempBloomData.Height = downSampleTexture.Desc.Height >> 1;
	
	commandList->BindComputePipeline(BloomThreshold);
	commandList->SetLocalData(sizeof(BloomData), &tempBloomData);
	
	commandList->BindAsRWTexture(0, downSampleTexture.RenderTextures[0], mipLevel);
	
	commandList->Dispatch(
		std::max(1u, downSampleTexture.Desc.Width / 8 + 1),
		std::max(1u, downSampleTexture.Desc.Height / 8 + 1),
		1
	);
	
	commandList->BindComputePipeline(BloomDownScale);
	
	std::vector<u32> widths(downSampleTexture.RenderTextures[0].Desc.NumMips);
	std::vector<u32> heights(downSampleTexture.RenderTextures[0].Desc.NumMips);
	widths[mipLevel] = downSampleTexture.Desc.Width;
	heights[mipLevel] = downSampleTexture.Desc.Height;
	while (mipLevel < downSampleTexture.RenderTextures[0].Desc.NumMips - 1)
	{
		commandList->SetLocalData(sizeof(BloomData), &tempBloomData);
	
		commandList->BindTexture(0, downSampleTexture.RenderTextures[0], mipLevel);
	
		commandList->BindAsRWTexture(0, downSampleTexture.RenderTextures[0], mipLevel + 1);
	
		commandList->Dispatch(
			std::max(1u, tempBloomData.Width / 8 + 1),
			std::max(1u, tempBloomData.Height / 8 + 1),
			1
		);
	
		mipLevel += 1;
		widths[mipLevel] = tempBloomData.Width;
		heights[mipLevel] = tempBloomData.Height;
		tempBloomData.Width = tempBloomData.Width >> 1;
		tempBloomData.Height = tempBloomData.Height >> 1;
	
	}

	commandList->BindComputePipeline(BloomUpScale);
	
	while (mipLevel >= 1)
	{
		tempBloomData.Width = widths[mipLevel - 1];
		tempBloomData.Height = heights[mipLevel - 1];
		commandList->SetLocalData(sizeof(BloomData), &tempBloomData);
	
		commandList->BindTexture(0, upSampleTexture.RenderTextures[0], mipLevel);
	
		commandList->BindAsRWTexture(0, downSampleTexture.RenderTextures[0], mipLevel - 1);
		commandList->BindAsRWTexture(1, upSampleTexture.RenderTextures[0], mipLevel - 1);
	
		commandList->Dispatch(
			std::max(1u, tempBloomData.Width / 8 + 1),
			std::max(1u, tempBloomData.Height / 8 + 1),
			1
		);
	
		mipLevel -= 1;
	}
	
	commandList->BindComputePipeline(BloomComposite);
	commandList->BindAsRWTexture(0, upSampleTexture.RenderTextures[0], 0);
	commandList->BindAsRWTexture(1, modifyTarget.RenderTextures[0]);
	
	commandList->Dispatch(
		std::max(1u, modifyTarget.Desc.Width / 8 + 1),
		std::max(1u, modifyTarget.Desc.Height / 8 + 1),
		1
	);
}

}