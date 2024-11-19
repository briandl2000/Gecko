#include "ExampleComputePass.h"

#include "Defines.h"
#include "Rendering/Backend/CommandList.h"
#include "Rendering/Frontend/Renderer/Renderer.h"

const void ExampleComputePass::SubInit(const Gecko::Platform::AppInfo& appInfo, Gecko::ResourceManager* resourceManager,
	const ConfigData& dependencies)
{
	// Simple colour output Compute Pipeline
	{
		Gecko::ComputePipelineDesc computePipelineDesc;
		// ComputeShaderPath should start in a subdirectory of WorkingDir, or be a file in WorkingDir
		computePipelineDesc.ComputeShaderPath = "Shaders/ExampleCompute.gsh";
		// All example shaders use HLSL shader version 5.1
		computePipelineDesc.ShaderVersion = "5_1";
		// We need only one Read write resource mainy the output texture
		computePipelineDesc.PipelineReadWriteResources = 
		{
			Gecko::PipelineResource::Texture(Gecko::ShaderVisibility::Compute, 0)
		};

		m_ExamplePipelineHandle = resourceManager->CreateComputePipeline(computePipelineDesc);
	}

	Gecko::RenderTargetDesc renderTargetDesc;
	// Unless you have a specific reason for wanting a differently sized render target, it usually makes sense to use the window resolution as the
	// render target resolution
	renderTargetDesc.Width = appInfo.Width;
	renderTargetDesc.Height = appInfo.Height;
	renderTargetDesc.NumRenderTargets = 1;
	// This for-loop obviously only executes once for a single render target, but it shows how you might set it up for multiple render targets
	for (Gecko::u32 i = 0; i < renderTargetDesc.NumRenderTargets; i++)
	{
		renderTargetDesc.RenderTargetClearValues[i].Values[0] = 0.f;
		renderTargetDesc.RenderTargetClearValues[i].Values[1] = 0.f;
		renderTargetDesc.RenderTargetClearValues[i].Values[2] = 0.f;
		renderTargetDesc.RenderTargetClearValues[i].Values[3] = 0.f;
	}
	// If the pipeline renders to the screen and is not an intermediate step (which is the case for this pipeline),
	// output format should usually be RGB float.
	// This pipeline only renders to the first texture in the render target, so only the first format needs to be defined
	renderTargetDesc.RenderTargetFormats[0] = Gecko::DataFormat::R32G32B32A32_FLOAT;

	m_OutputHandle = resourceManager->CreateRenderTarget(renderTargetDesc, "ToneMappingGammaCorrection", true);
}

const void ExampleComputePass::Render(const Gecko::SceneRenderInfo& sceneRenderInfo, Gecko::ResourceManager* resourceManager,
	const Gecko::Renderer* renderer, Gecko::Ref<Gecko::CommandList> commandList)
{
	// Use stored handle to fetch output render target
	Gecko::RenderTarget outputTarget = resourceManager->GetRenderTarget(m_OutputHandle);

	// Use stored handle to fetch the actual pipeline
	Gecko::ComputePipeline exampleComputePipeline = resourceManager->GetComputePipeline(m_ExamplePipelineHandle);

	// Bind the pipeline
	commandList->BindComputePipeline(exampleComputePipeline);
	// Bind the first texture of the output target as a Read/Write texture, since it is going to be rendered to
	commandList->BindAsRWTexture(0, outputTarget.RenderTextures[0]);
	// Dispatch the render (or in this case compute) call.
	// This call uses (width / 8 + 1) * (height / 8 + 1) * 1 cores, because the shader (ExampleComputeShader.gsh) specifies that it runs over 
	// 8x8x1 threads as a group. Different thread groups and core numbers can be tried to test for performance improvements
	commandList->Dispatch(
		std::max(1u, outputTarget.Desc.Width / 8 + 1),
		std::max(1u, outputTarget.Desc.Height / 8 + 1),
		1
	);
}
