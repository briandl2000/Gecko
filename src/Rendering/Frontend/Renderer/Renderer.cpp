#include "Rendering/Frontend/Renderer/Renderer.h"

#include "Core/Logger.h"
#include "Core/Asserts.h"
#include "Rendering/Backend/Device.h"
#include "Rendering/Backend/CommandList.h"
#include "Rendering/Frontend/Scene/Scene.h"

#include "stb_image.h"
#include "Core/Platform.h"

#include <random>


namespace Gecko {

const VertexLayout fullScreenQuadVertexLayout({
	{DataFormat::R32G32_FLOAT, "POSITION"},
});


void Renderer::Init(Platform::AppInfo& info, ResourceManager* resourceManager, Device* _device)
{
	m_Info = info;

	device = _device;

	m_ResourceManager = resourceManager;

	// FullScreenTexture Graphics Pipeline
	{
		GraphicsPipelineDesc pipelineDesc;
		pipelineDesc.VertexShaderPath = "Shaders/FullScreenTexture.gsh";
		pipelineDesc.PixelShaderPath = "Shaders/FullScreenTexture.gsh";
		pipelineDesc.ShaderVersion = "5_1";
		pipelineDesc.VertexLayout = fullScreenQuadVertexLayout;
		pipelineDesc.NumRenderTargets = 1;
		pipelineDesc.RenderTextureFormats[0] = DataFormat::R8G8B8A8_UNORM; // Albedo
		pipelineDesc.CullMode = CullMode::Back;

		pipelineDesc.PipelineResources = {
			PipelineResource::Texture(ShaderType::Pixel, 0)
		};

		pipelineDesc.SamplerDescs = {
			{ShaderType::Pixel, SamplerFilter::Point, },
		};

		FullScreenTexturePipelineHandle = m_ResourceManager->CreateGraphicsPipeline(pipelineDesc);
	}

	// Make The quad
	{
		float vertices[] = {
			-1.f, -1.f,
			-1.f,  3.f,
			 3.f, -1.f,
		};

		Gecko::VertexBufferDesc vertexDesc;
		vertexDesc.Layout = fullScreenQuadVertexLayout;
		vertexDesc.NumVertices = static_cast<u32>(3);
		vertexDesc.MemoryType = MemoryType::Dedicated;
		
		u16 indices[] = {
			0, 1, 2
		};

		Gecko::IndexBufferDesc indexDesc;
		indexDesc.IndexFormat = DataFormat::R16_UINT;
		indexDesc.NumIndices = 3;
		indexDesc.MemoryType = MemoryType::Dedicated;

		quadMeshHandle = m_ResourceManager->CreateMesh(vertexDesc, indexDesc, vertices, indices);
	}
}

Renderer::~Renderer()
{
}

void Renderer::Shutdown()
{
	m_RenderPassStack.clear();
	m_RenderPasses.clear();
	device = nullptr;
	m_ResourceManager = nullptr;
}

void Renderer::RenderScene(const SceneRenderInfo& sceneRenderInfo)
{
	// Create a command list for this frame
	Gecko::Ref<Gecko::CommandList> commandList = device->CreateGraphicsCommandList();

	// Render the render passes
	for (RenderPassHandle renderPassHandle : m_RenderPassStack)
	{
		const auto renderPass = GetRenderPassByHandle(renderPassHandle);
		renderPass->Render(sceneRenderInfo, m_ResourceManager, this, commandList);
	}

	// Render the output from the final render pass to the back buffer
	// TODO: move this to the present function
	RenderTarget inputTarget = m_ResourceManager->GetRenderTarget(GetRenderPassByHandle(m_RenderPassStack.back())->GetOutputHandle());
	RenderTarget renderTarget = device->GetCurrentBackBuffer();
	
	GraphicsPipeline FullScreenTexturePipeline = m_ResourceManager->GetGraphicsPipeline(FullScreenTexturePipelineHandle);
	commandList->BindGraphicsPipeline(FullScreenTexturePipeline);
	Mesh quadMesh = m_ResourceManager->GetMesh(quadMeshHandle);
	commandList->BindVertexBuffer(quadMesh.VertexBuffer);
	commandList->BindIndexBuffer(quadMesh.IndexBuffer);
	commandList->BindTexture(0, inputTarget.RenderTextures[0]);
	commandList->BindRenderTarget(renderTarget);

	commandList->Draw(3);

	// Execute command list and display it
	device->ExecuteGraphicsCommandListAndFlip(commandList);
}

void Renderer::Present() 
{

}

}