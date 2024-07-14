#include "Rendering/Frontend/Renderer/RenderPasses/DeferredPBRPass.h"

#include "Rendering/Backend/CommandList.h"

#include "Rendering/Frontend/ResourceManager/ResourceManager.h"
#include <stb_image.h>

namespace Gecko
{

const void DeferredPBRPass::Init(Platform::AppInfo& appInfo, ResourceManager* resourceManager)
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
		computePipelineDesc.ComputeShaderPath = "Shaders/PBRShader";
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
	PBROutputHandle = resourceManager->CreateRenderTarget(PBROutputDesc, "PBROutput", true);

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
	
}

const void DeferredPBRPass::Render(const SceneRenderInfo& sceneRenderInfo, ResourceManager* resourceManager, Ref<CommandList> commandList)
{

	Ref<RenderTarget> GBuffer = resourceManager->GetRenderTarget(resourceManager->GetRenderTargetHandle("GBuffer"));
	Ref<RenderTarget> ShadowMap = resourceManager->GetRenderTarget(resourceManager->GetRenderTargetHandle("ShadowMap"));
	Ref<RenderTarget> PBROutput = resourceManager->GetRenderTarget(PBROutputHandle);

	Ref<Texture> BRDFLUTTexture = resourceManager->GetTexture(BRDFLUTTextureHandle);

	ComputePipeline PBRPipeline = resourceManager->GetComputePipeline(PBRPipelineHandle);

	// PBR Deferred rendering pass
	PBRData pbrData;
	pbrData.width = PBROutput->Desc.Width;
	pbrData.height = PBROutput->Desc.Height;
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
		Ref<Texture> environmentTexture = resourceManager->GetTexture(environmentMap.EnvironmentTextureHandle);
		Ref<Texture> irradianceTexture = resourceManager->GetTexture(environmentMap.IrradianceTextureHandle);
		commandList->BindTexture(1, environmentTexture);
		commandList->BindTexture(2, irradianceTexture);
		commandList->BindTexture(3, ShadowMap, RenderTargetType::TargetDepth);
		
		commandList->Dispatch(pbrData.width / 8 + 1, pbrData.height / 8 + 1, 1);
	}
}

}