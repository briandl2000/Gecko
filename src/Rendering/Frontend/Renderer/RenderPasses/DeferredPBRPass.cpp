#include "Rendering/Frontend/Renderer/RenderPasses/DeferredPBRPass.h"

#include "Rendering/Backend/CommandList.h"

#include "Rendering/Frontend/ResourceManager/ResourceManager.h"
#include "Rendering/Frontend/Renderer/Renderer.h"
#include <stb_image.h>

namespace Gecko
{

const void DeferredPBRPass::SubInit(const Platform::AppInfo& appInfo, ResourceManager* resourceManager, const InputData& dependencies)
{

	// PBR Compute Pipeline
	{
		std::vector<SamplerDesc> computeSamplerShaderDescs =
		{
			{
				ShaderVisibility::Pixel,
				SamplerFilter::Linear,
				SamplerWrapMode::Clamp,
				SamplerWrapMode::Clamp,
				SamplerWrapMode::Clamp,
			},
			{
				ShaderVisibility::Pixel,
				SamplerFilter::Point,
				SamplerWrapMode::Clamp,
				SamplerWrapMode::Clamp,
				SamplerWrapMode::Clamp,
			}
		};

		ComputePipelineDesc computePipelineDesc;
		computePipelineDesc.ComputeShaderPath = "Shaders/PBRShader.gsh";
		computePipelineDesc.ShaderVersion = "5_1";
		computePipelineDesc.DynamicCallData.BufferLocation = 1;
		computePipelineDesc.DynamicCallData.Size = sizeof(PBRData);
		computePipelineDesc.SamplerDescs = computeSamplerShaderDescs;
		computePipelineDesc.NumTextures = 4;
		computePipelineDesc.NumUAVs = 6;
		computePipelineDesc.NumConstantBuffers = 1;

		PBRPipelineHandle = resourceManager->CreateComputePipeline(computePipelineDesc);
	}
	
	Gecko::RenderTargetDesc PBROutputDesc;
	PBROutputDesc.AllowRenderTargetTexture = true;
	PBROutputDesc.Width = appInfo.Width;
	PBROutputDesc.Height = appInfo.Height;
	PBROutputDesc.NumRenderTargets = 1;
	for (u32 i = 0; i < PBROutputDesc.NumRenderTargets; i++)
	{
		PBROutputDesc.RenderTargetClearValues[i].Values[0] = 0.f;
		PBROutputDesc.RenderTargetClearValues[i].Values[1] = 0.f;
		PBROutputDesc.RenderTargetClearValues[i].Values[2] = 0.f;
		PBROutputDesc.RenderTargetClearValues[i].Values[3] = 0.f;
	}
	PBROutputDesc.RenderTargetFormats[0] = Gecko::Format::R32G32B32A32_FLOAT; // output
	m_OutputHandle = resourceManager->CreateRenderTarget(PBROutputDesc, "PBROutput", true);

	int width, height, n;
	unsigned char* image = stbi_load(Platform::GetLocalPath("Assets/BRDF_LUT.png").c_str(), &width, &height, &n, 4);
	Gecko::TextureDesc textureDesc;

	textureDesc.Width = width;
	textureDesc.Height = height;
	textureDesc.Type = Gecko::TextureType::Tex2D;
	textureDesc.Format = Gecko::Format::R8G8B8A8_UNORM;
	textureDesc.NumMips = 1;
	textureDesc.NumArraySlices = 1;
	BRDFLUTTextureHandle = resourceManager->CreateTexture(textureDesc, image);
	
	stbi_image_free(image);
	
	m_Input = dependencies;
}

const void DeferredPBRPass::Render(const SceneRenderInfo& sceneRenderInfo, ResourceManager* resourceManager,
	const Renderer* renderer, Ref<CommandList> commandList)
{

	RenderTarget GBuffer = resourceManager->GetRenderTarget(renderer->GetRenderPassByHandle(m_Input.GeoPass)->GetOutputHandle());
	RenderTarget ShadowMap = resourceManager->GetRenderTarget(renderer->GetRenderPassByHandle(m_Input.ShadowPass)->GetOutputHandle());
	RenderTarget PBROutput = resourceManager->GetRenderTarget(m_OutputHandle);

	Texture BRDFLUTTexture = resourceManager->GetTexture(BRDFLUTTextureHandle);

	ComputePipeline PBRPipeline = resourceManager->GetComputePipeline(PBRPipelineHandle);

	// PBR Deferred rendering pass
	PBRData pbrData;
	pbrData.width = PBROutput.Desc.Width;
	pbrData.height = PBROutput.Desc.Height;
	{
		commandList->BindComputePipeline(PBRPipeline);

		u32 currentBackBufferIndex = resourceManager->GetCurrentBackBufferIndex();
		commandList->BindConstantBuffer(0, resourceManager->SceneDataBuffer[currentBackBufferIndex]);
		commandList->SetDynamicCallData(sizeof(PBRData), &pbrData);
		commandList->BindAsRWTexture(0, PBROutput, Gecko::RenderTargetType::Target0);
		commandList->BindAsRWTexture(1, GBuffer, Gecko::RenderTargetType::Target0);
		commandList->BindAsRWTexture(2, GBuffer, Gecko::RenderTargetType::Target1);
		commandList->BindAsRWTexture(3, GBuffer, Gecko::RenderTargetType::Target2);
		commandList->BindAsRWTexture(4, GBuffer, Gecko::RenderTargetType::Target3);
		commandList->BindAsRWTexture(5, GBuffer, Gecko::RenderTargetType::Target4);

		commandList->BindTexture(0, BRDFLUTTexture);
		EnvironmentMap environmentMap = resourceManager->GetEnvironmentMap(sceneRenderInfo.EnvironmentMap);
		Texture environmentTexture = resourceManager->GetTexture(environmentMap.EnvironmentTextureHandle);
		Texture irradianceTexture = resourceManager->GetTexture(environmentMap.IrradianceTextureHandle);
		commandList->BindTexture(1, environmentTexture);
		commandList->BindTexture(2, irradianceTexture);
		commandList->BindTexture(3, ShadowMap, RenderTargetType::TargetDepth);
		
		commandList->Dispatch(pbrData.width / 8 + 1, pbrData.height / 8 + 1, 1);
	}
}

}