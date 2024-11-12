#include "Rendering/Frontend/Renderer/RenderPasses/DeferredPBRPass.h"

#include "Rendering/Backend/CommandList.h"

#include "Rendering/Frontend/ResourceManager/ResourceManager.h"
#include "Rendering/Frontend/Renderer/Renderer.h"
#include <stb_image.h>

namespace Gecko
{

const void DeferredPBRPass::SubInit(const Platform::AppInfo& appInfo, ResourceManager* resourceManager, const ConfigData& dependencies)
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
		computePipelineDesc.PipelineReadOnlyBuffers = { 
			PipelineBuffer::ConstantBuffer(ShaderVisibility::Compute, 0), 
			PipelineBuffer::LocalData(ShaderVisibility::Compute, 1, sizeof(PBRData)) 
		};
		computePipelineDesc.SamplerDescs = computeSamplerShaderDescs;
		computePipelineDesc.NumTextures = 4;
		computePipelineDesc.NumUAVs = 6;

		PBRPipelineHandle = resourceManager->CreateComputePipeline(computePipelineDesc);
	}
	
	Gecko::RenderTargetDesc PBROutputDesc;
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
	PBROutputDesc.RenderTargetFormats[0] = DataFormat::R32G32B32A32_FLOAT; // output
	m_OutputHandle = resourceManager->CreateRenderTarget(PBROutputDesc, "PBROutput", true);

	int width, height, n;
	unsigned char* image = stbi_load(Platform::GetLocalPath("Assets/BRDF_LUT.png").c_str(), &width, &height, &n, 4);
	TextureDesc textureDesc;

	textureDesc.Width = width;
	textureDesc.Height = height;
	textureDesc.Type = TextureType::Tex2D;
	textureDesc.Format = DataFormat::R8G8B8A8_UNORM;
	textureDesc.NumMips = 1;
	textureDesc.NumArraySlices = 1;
	BRDFLUTTextureHandle = resourceManager->CreateTexture(textureDesc, image);
	
	stbi_image_free(image);
	
	m_ConfigData = dependencies;
}

const void DeferredPBRPass::Render(const SceneRenderInfo& sceneRenderInfo, ResourceManager* resourceManager,
	const Renderer* renderer, Ref<CommandList> commandList)
{

	RenderTarget GBuffer = resourceManager->GetRenderTarget(renderer->GetRenderPassByHandle(m_ConfigData.GeoPass)->GetOutputHandle());
	RenderTarget ShadowMap = resourceManager->GetRenderTarget(renderer->GetRenderPassByHandle(m_ConfigData.ShadowPass)->GetOutputHandle());
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
		commandList->SetLocalData(sizeof(PBRData), &pbrData);
		commandList->BindAsRWTexture(0, PBROutput.RenderTextures[0]);
		commandList->BindAsRWTexture(1, GBuffer.RenderTextures[0]);
		commandList->BindAsRWTexture(2, GBuffer.RenderTextures[1]);
		commandList->BindAsRWTexture(3, GBuffer.RenderTextures[2]);
		commandList->BindAsRWTexture(4, GBuffer.RenderTextures[3]);
		commandList->BindAsRWTexture(5, GBuffer.RenderTextures[4]);

		commandList->BindTexture(0, BRDFLUTTexture);
		EnvironmentMap environmentMap = resourceManager->GetEnvironmentMap(sceneRenderInfo.EnvironmentMap);
		Texture environmentTexture = resourceManager->GetTexture(environmentMap.EnvironmentTextureHandle);
		Texture irradianceTexture = resourceManager->GetTexture(environmentMap.IrradianceTextureHandle);
		commandList->BindTexture(1, environmentTexture);
		commandList->BindTexture(2, irradianceTexture);
		commandList->BindTexture(3, ShadowMap.DepthTexture);
		
		commandList->Dispatch(pbrData.width / 8 + 1, pbrData.height / 8 + 1, 1);
	}
}

}