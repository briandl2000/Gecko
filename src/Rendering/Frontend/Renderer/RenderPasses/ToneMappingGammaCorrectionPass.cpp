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
			PipelineResource::Texture(ShaderType::Compute, 0)
		};
		computePipelineDesc.PipelineReadOnlyResources = 
		{
			PipelineResource::ConstantBuffer(ShaderType::Compute, 0)
		};

		TonemapAndGammaCorrectPipelineHandle = resourceManager->CreateComputePipeline(computePipelineDesc);
	}

	// This pass modifies the render target output of the previous pass in-place,
	// so no need to create a new output target
	m_OutputHandle = dependencies.PrevPassOutput;
}

const void ToneMappingGammaCorrectionPass::Render(const SceneRenderInfo& sceneRenderInfo, ResourceManager* resourceManager,
	const Renderer* renderer, Ref<CommandList> commandList)
{
	RenderTarget outputTarget = resourceManager->GetRenderTarget(m_OutputHandle);

	ComputePipeline TonemapAndGammaCorrectPipeline = resourceManager->GetComputePipeline(TonemapAndGammaCorrectPipelineHandle);

	commandList->BindComputePipeline(TonemapAndGammaCorrectPipeline);
	commandList->BindAsRWTexture(0, outputTarget.RenderTextures[0]);
	u32 currentBackBufferIndex = resourceManager->GetCurrentBackBufferIndex();
	commandList->BindConstantBuffer(0, resourceManager->SceneDataBuffer[currentBackBufferIndex]);
	commandList->Dispatch(
		std::max(1u, outputTarget.Desc.Width / 8 + 1),
		std::max(1u, outputTarget.Desc.Height / 8 + 1),
		1
	);
}


}