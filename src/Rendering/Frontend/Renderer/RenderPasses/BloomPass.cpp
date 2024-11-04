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
		std::vector<SamplerDesc> computeSamplerShaderDescs =
		{
			{
				ShaderVisibility::Pixel,
				SamplerFilter::Linear,
				SamplerWrapMode::Clamp,
				SamplerWrapMode::Clamp,
				SamplerWrapMode::Clamp,
			}
		};

		ComputePipelineDesc computePipelineDesc;
		computePipelineDesc.ComputeShaderPath = "Shaders/Bloom/BloomDownScale.gsh";
		computePipelineDesc.ShaderVersion = "5_1";
		computePipelineDesc.DynamicCallData.BufferLocation = 0;
		computePipelineDesc.DynamicCallData.Size = sizeof(BloomData);
		computePipelineDesc.SamplerDescs = computeSamplerShaderDescs;
		computePipelineDesc.NumTextures = 1;
		computePipelineDesc.NumUAVs = 1;

		m_DownScalePipelineHandle = resourceManager->CreateComputePipeline(computePipelineDesc);
	}

	// BloomUpScale Compute Pipeline
	{
		std::vector<SamplerDesc> computeSamplerShaderDescs =
		{
			{
				ShaderVisibility::Pixel,
				SamplerFilter::Linear,
				SamplerWrapMode::Clamp,
				SamplerWrapMode::Clamp,
				SamplerWrapMode::Clamp,
			}
		};

		ComputePipelineDesc computePipelineDesc;
		computePipelineDesc.ComputeShaderPath = "Shaders/Bloom/BloomUpScale.gsh";
		computePipelineDesc.ShaderVersion = "5_1";
		computePipelineDesc.DynamicCallData.BufferLocation = 0;
		computePipelineDesc.DynamicCallData.Size = sizeof(BloomData);
		computePipelineDesc.SamplerDescs = computeSamplerShaderDescs;
		computePipelineDesc.NumTextures = 1;
		computePipelineDesc.NumUAVs = 2;


		m_UpScalePipelineHandle = resourceManager->CreateComputePipeline(computePipelineDesc);

	}

	// BloomThreshold Compute Pipeline
	{

		ComputePipelineDesc computePipelineDesc;
		computePipelineDesc.ComputeShaderPath = "Shaders/Bloom/BloomThreshold.gsh";
		computePipelineDesc.ShaderVersion = "5_1";
		computePipelineDesc.DynamicCallData.BufferLocation = 0;
		computePipelineDesc.DynamicCallData.Size = sizeof(BloomData);
		computePipelineDesc.NumUAVs = 1;

		m_ThresholdPipelineHandle = resourceManager->CreateComputePipeline(computePipelineDesc);

	}

	// BloomComposite Compute Pipeline
	{
		ComputePipelineDesc computePipelineDesc;
		computePipelineDesc.ComputeShaderPath = "Shaders/Bloom/BloomComposite.gsh";
		computePipelineDesc.ShaderVersion = "5_1";
		computePipelineDesc.NumUAVs = 3;

		m_CompositePipelineHandle = resourceManager->CreateComputePipeline(computePipelineDesc);
	}

	RenderTargetDesc renderTargetDesc;

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
	renderTargetDesc.RenderTargetFormats[0] = Gecko::Format::R32G32B32A32_FLOAT;

	m_OutputHandle = resourceManager->CreateRenderTarget(renderTargetDesc, "BloomOutput", true);

	renderTargetDesc.NumMips[0] = 8;
	m_DownScaleRenderTargetHandle = resourceManager->CreateRenderTarget(renderTargetDesc, "m_DownScaleRenderTargetHandle", true);
	m_UpScaleRenderTargetHandle = resourceManager->CreateRenderTarget(renderTargetDesc, "m_UpScaleRenderTargetHandle", true);

	m_BloomData.Width = appInfo.Width;
	m_BloomData.Height = appInfo.Height;
	m_BloomData.Threshold = .9f;

	m_ConfigData = dependencies;
}

const void BloomPass::Render(const SceneRenderInfo& sceneRenderInfo, ResourceManager* resourceManager,
	const Renderer* renderer, Ref<CommandList> commandList)
{

	RenderTarget inputTarget = resourceManager->GetRenderTarget(renderer->GetRenderPassByHandle(m_ConfigData.PrevPass)->GetOutputHandle());
	RenderTarget downSampleTexture = resourceManager->GetRenderTarget(m_DownScaleRenderTargetHandle);
	RenderTarget upSampleTexture = resourceManager->GetRenderTarget(m_UpScaleRenderTargetHandle);
	RenderTarget outputTarget = resourceManager->GetRenderTarget(m_OutputHandle);

	ComputePipeline BloomDownScale = resourceManager->GetComputePipeline(m_DownScalePipelineHandle);
	ComputePipeline BloomUpScale = resourceManager->GetComputePipeline(m_UpScalePipelineHandle);
	ComputePipeline BloomThreshold = resourceManager->GetComputePipeline(m_ThresholdPipelineHandle);
	ComputePipeline BloomComposite = resourceManager->GetComputePipeline(m_CompositePipelineHandle);

	commandList->CopyTextureToTexture(
		inputTarget.RenderTextures[0],
		downSampleTexture.RenderTextures[0]
	);

	u32 mipLevel = 0;
	BloomData tempBloomData = m_BloomData;
	tempBloomData.Width = downSampleTexture.Desc.Width >> 1;
	tempBloomData.Height = downSampleTexture.Desc.Height >> 1;
	
	commandList->BindComputePipeline(BloomThreshold);
	commandList->SetDynamicCallData(sizeof(BloomData), &tempBloomData);
	
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
		commandList->SetDynamicCallData(sizeof(BloomData), &tempBloomData);
	
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
		commandList->SetDynamicCallData(sizeof(BloomData), &tempBloomData);
	
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
	commandList->BindAsRWTexture(1, inputTarget.RenderTextures[0]);
	commandList->BindAsRWTexture(2, outputTarget.RenderTextures[0]);
	
	commandList->Dispatch(
		std::max(1u, outputTarget.Desc.Width / 8 + 1),
		std::max(1u, outputTarget.Desc.Height / 8 + 1),
		1
	);
}

}