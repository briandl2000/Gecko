#include "Rendering/Frontend/Renderer/RenderPasses/BloomPass.h"

#include "Rendering/Backend/CommandList.h"

#include "Rendering/Frontend/ResourceManager/ResourceManager.h"
#include "Rendering/Frontend/Scene/Scene.h"

#include <algorithm>

namespace Gecko
{

const void BloomPass::Init(ResourceManager* resourceManager)
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
		computePipelineDesc.ComputeShaderPath = "Shaders/BloomDownScale";
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
		computePipelineDesc.ComputeShaderPath = "Shaders/BloomUpScale";
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
		computePipelineDesc.ComputeShaderPath = "Shaders/BloomThreshold";
		computePipelineDesc.DynamicCallData.BufferLocation = 0;
		computePipelineDesc.DynamicCallData.Size = sizeof(BloomData);
		computePipelineDesc.NumUAVs = 1;

		m_ThresholdPipelineHandle = resourceManager->CreateComputePipeline(computePipelineDesc);

	}

	// BloomComposite Compute Pipeline
	{
		ComputePipelineDesc computePipelineDesc;
		computePipelineDesc.ComputeShaderPath = "Shaders/BloomComposite";
		computePipelineDesc.NumUAVs = 3;

		m_CompositePipelineHandle = resourceManager->CreateComputePipeline(computePipelineDesc);
	}

	TextureDesc textureDesc;

	textureDesc.Width = 1920;
	textureDesc.Height = 1080;
	textureDesc.Type = TextureType::Tex2D;
	textureDesc.Format = Format::R32G32B32A32_FLOAT;
	textureDesc.NumMips = CalculateNumberOfMips(textureDesc.Width, textureDesc.Height);
	textureDesc.NumArraySlices = 1;

	m_DownScaleTextureHandle = resourceManager->CreateTexture(textureDesc);
	m_UpScaleTextureHandle = resourceManager->CreateTexture(textureDesc);

	RenderTargetDesc renderTargetDesc;

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
	renderTargetDesc.RenderTargetFormats[0] = Gecko::Format::R32G32B32A32_FLOAT;

	m_OutputTargetHandle = resourceManager->CreateRenderTarget(renderTargetDesc, "BloomOutput");

	m_BloomData.Width = 1920;
	m_BloomData.Height = 1080;
	m_BloomData.Threshold = .9f;
}

const void BloomPass::Render(const SceneDescriptor& sceneDescriptor, ResourceManager* resourceManager, Ref<CommandList> commandList)
{

	Ref<RenderTarget> inputTarget = resourceManager->GetRenderTarget(resourceManager->GetRenderTargetHandle("FXAAOutput"));
	Ref<Texture> downSampleTexture = resourceManager->GetTexture(m_DownScaleTextureHandle);
	Ref<Texture> upSampleTexture = resourceManager->GetTexture(m_UpScaleTextureHandle);
	Ref<RenderTarget> outputTarget = resourceManager->GetRenderTarget(m_OutputTargetHandle);

	ComputePipeline BloomDownScale = resourceManager->GetComputePipeline(m_DownScalePipelineHandle);
	ComputePipeline BloomUpScale = resourceManager->GetComputePipeline(m_UpScalePipelineHandle);
	ComputePipeline BloomThreshold = resourceManager->GetComputePipeline(m_ThresholdPipelineHandle);
	ComputePipeline BloomComposite = resourceManager->GetComputePipeline(m_CompositePipelineHandle);

	commandList->CopyFromRenderTarget(
		inputTarget, Gecko::RenderTargetType::Target0,
		downSampleTexture
	);

	u32 mipLevel = 0;
	BloomData tempBloomData = m_BloomData;
	tempBloomData.Width = downSampleTexture->Desc.Width >> 1;
	tempBloomData.Height = downSampleTexture->Desc.Height >> 1;

	commandList->BindComputePipeline(BloomThreshold);
	commandList->SetDynamicCallData(sizeof(BloomData), &tempBloomData);

	commandList->BindAsRWTexture(0, downSampleTexture, mipLevel);

	commandList->Dispatch(
		std::max(1u, downSampleTexture->Desc.Width / 8 + 1),
		std::max(1u, downSampleTexture->Desc.Height / 8 + 1),
		1
	);

	commandList->BindComputePipeline(BloomDownScale);

	std::vector<u32> widths(downSampleTexture->Desc.NumMips);
	std::vector<u32> heights(downSampleTexture->Desc.NumMips);
	widths[mipLevel] = downSampleTexture->Desc.Width;
	heights[mipLevel] = downSampleTexture->Desc.Height;
	while (mipLevel < downSampleTexture->Desc.NumMips - 1)
	{
		commandList->SetDynamicCallData(sizeof(BloomData), &tempBloomData);

		commandList->BindTexture(0, downSampleTexture, mipLevel);

		commandList->BindAsRWTexture(0, downSampleTexture, mipLevel + 1);

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

		commandList->BindTexture(0, upSampleTexture, mipLevel);

		commandList->BindAsRWTexture(0, downSampleTexture, mipLevel - 1);
		commandList->BindAsRWTexture(1, upSampleTexture, mipLevel - 1);

		commandList->Dispatch(
			std::max(1u, tempBloomData.Width / 8 + 1),
			std::max(1u, tempBloomData.Height / 8 + 1),
			1
		);

		mipLevel -= 1;
	}

	commandList->BindComputePipeline(BloomComposite);
	commandList->BindAsRWTexture(0, upSampleTexture, 0);
	commandList->BindAsRWTexture(1, inputTarget, Gecko::RenderTargetType::Target0);
	commandList->BindAsRWTexture(2, outputTarget, Gecko::RenderTargetType::Target0);

	commandList->Dispatch(
		std::max(1u, outputTarget->Desc.Width / 8 + 1),
		std::max(1u, outputTarget->Desc.Height / 8 + 1),
		1
	);
}

}