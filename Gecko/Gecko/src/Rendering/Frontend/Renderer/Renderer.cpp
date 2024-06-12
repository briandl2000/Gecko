#include "Renderer.h"

#include "Logging/Logger.h"
#include "Logging/Asserts.h"
#include "Rendering/Backend/Device.h"
#include "Rendering/Backend/CommandList.h"
#include "Rendering/Frontend/Scene/Scene.h"

#include "stb_image.h"
#include <imgui.h>
#include "Platform/Platform.h"

#include "glm/matrix.hpp"
#include <random>


namespace Gecko {

const VertexLayout fullScreenQuadVertexLayout({
	{Format::R32G32_FLOAT, "POSITION"},
});


void Renderer::Init(Platform::AppInfo info, ResourceManager* resourceManager, Device* _device)
{
	device = _device;

	m_ResourceManager = resourceManager;

	//{
	//	Gecko::TextureDesc RaytraceTargetDesc;

	//	RaytraceTargetDesc.Width = info.FullScreenWidth;
	//	RaytraceTargetDesc.Height = info.FullScreenHeight;
	//	RaytraceTargetDesc.Type = Gecko::TextureType::Tex2D;
	//	RaytraceTargetDesc.Format = Gecko::Format::R32_FLOAT;
	//	RaytraceTargetDesc.NumMips = 1;
	//	RaytraceTargetDesc.NumArraySlices = 1;
	//	RaytraceTarget = device->CreateTexture(RaytraceTargetDesc);

	//}

	// FullScreenTexture Graphics Pipeline
	{
		std::vector<ShaderVisibility> textureShaderVisibilities =
		{
			ShaderVisibility::Pixel,
		};

		std::vector<SamplerDesc> samplerShaderDescs =
		{
			{
				ShaderVisibility::Pixel,
				SamplerFilter::Point,
			},
		};

		GraphicsPipelineDesc pipelineDesc;
		pipelineDesc.VertexShaderPath = "Shaders/FullScreenTextureVertex";
		pipelineDesc.PixelShaderPath = "Shaders/FullScreenTexturePixel";
		pipelineDesc.VertexLayout = fullScreenQuadVertexLayout;
		pipelineDesc.RenderTargetFormats[0] = Format::R8G8B8A8_UNORM; // Albedo
		pipelineDesc.CullMode = CullMode::Back;

		pipelineDesc.TextureShaderVisibilities = textureShaderVisibilities;

		pipelineDesc.SamplerDescs = samplerShaderDescs;

		FullScreenTexturePipelineHandle = m_ResourceManager->CreateGraphicsPipeline(pipelineDesc);
	}


	// Make The quad
	{

		glm::vec2 vertices[] = {
			{-1.f, -1.f},
			{-1.f,  3.f},
			{ 3.f, -1.f},
		};

		Gecko::VertexBufferDesc vertexDesc;
		vertexDesc.Layout = fullScreenQuadVertexLayout;
		vertexDesc.VertexData = &vertices;
		vertexDesc.NumVertices = static_cast<u32>(sizeof(vertices) / sizeof(glm::vec2));

		u16 indices[] = {
			0, 1, 2
		};

		Gecko::IndexBufferDesc indexDesc;
		indexDesc.IndexFormat = Gecko::Format::R16_UINT;
		indexDesc.NumIndices = 3;
		indexDesc.IndexData = indices;

		quadMeshHandle = m_ResourceManager->CreateMesh(vertexDesc, indexDesc, false);
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

void Renderer::RenderScene(SceneDescriptor& sceneDescriptor)
{
	/*TLASRefitDesc refitDesc;

	for (u32 i = 0; i < sceneDescriptor.Meshes.size(); i++)
	{
		const MeshInstanceDescriptor& meshInstanceDescriptor = sceneDescriptor.Meshes[i];

		glm::mat4 transform = sceneDescriptor.NodeTransformMatrices[meshInstanceDescriptor.NodeTransformMatrixIndex];
		BLAS blas = m_ResourceManager->GetMesh(meshInstanceDescriptor.MeshInstance.MeshHandle).BLAS;

		refitDesc.BLASInstances.push_back({blas, transform});
	}*/

	// Upload Scene Data for this frame
	m_ResourceManager->SceneData->useRaytracing = false;
	m_ResourceManager->SceneData->ViewMatrix = sceneDescriptor.NodeTransformMatrices[sceneDescriptor.Camera.ViewMatrixIndex];
	m_ResourceManager->SceneData->CameraPosition = glm::vec3(m_ResourceManager->SceneData->ViewMatrix * glm::vec4(0.f, 0.f, 0.f, 1.f));
	m_ResourceManager->SceneData->ViewOrientation = glm::mat4(glm::mat3(m_ResourceManager->SceneData->ViewMatrix));
	m_ResourceManager->SceneData->InvViewOrientation = glm::inverse(m_ResourceManager->SceneData->ViewOrientation);
	m_ResourceManager->SceneData->ProjectionMatrix = sceneDescriptor.Camera.Projection;
	m_ResourceManager->SceneData->InvViewMatrix = glm::inverse(m_ResourceManager->SceneData->ViewMatrix);
	m_ResourceManager->SceneData->invProjectionMatrix = glm::inverse(m_ResourceManager->SceneData->ProjectionMatrix);

	glm::mat4 LighDirectionMatrix = glm::eulerAngleYX(0.f, 0.f);
	LighDirectionMatrix = glm::translate(glm::mat4(1.), m_ResourceManager->SceneData->CameraPosition - m_ResourceManager->SceneData->LightDirection * 50.f) * LighDirectionMatrix;
	m_ResourceManager->SceneData->ShadowMapProjection = glm::ortho(-30.f, 30.f, -30.f, 30.f, -100.f, 100.f) * glm::inverse(LighDirectionMatrix);
	m_ResourceManager->SceneData->ShadowMapProjectionInv = glm::inverse(m_ResourceManager->SceneData->ShadowMapProjection);
	m_ResourceManager->SceneData->LightDirection = glm::normalize(glm::vec3(LighDirectionMatrix * glm::vec4(0., 0., -1., 0.)));


	// Create a command list for this frame
	Gecko::Ref<Gecko::CommandList> commandList = device->CreateGraphicsCommandList();

	// Render the render passes
	for (const Ref<RenderPass>& renderPass : m_RenderPassStack)
	{
		renderPass->Render(sceneDescriptor, m_ResourceManager, commandList);
	}

	// Render to the back buffer
	// TODO: move this to the present function
	Ref<RenderTarget> inputTarget = m_ResourceManager->GetRenderTarget(m_ResourceManager->GetRenderTargetHandle("ToneMappingGammaCorrection"));
	Ref<RenderTarget> renderTarget = device->GetCurrentBackBuffer();
	
	GraphicsPipeline FullScreenTexturePipeline = m_ResourceManager->GetGraphicsPipeline(FullScreenTexturePipelineHandle);
	commandList->BindGraphicsPipeline(FullScreenTexturePipeline);
	Mesh quadMesh = m_ResourceManager->GetMesh(quadMeshHandle);
	commandList->BindVertexBuffer(quadMesh.VertexBuffer);
	commandList->BindIndexBuffer(quadMesh.IndexBuffer);
	commandList->BindTexture(0, inputTarget, Gecko::RenderTargetType::Target0);
	commandList->BindRenderTarget(renderTarget);

	commandList->Draw(3);

	// TODO: move this to its own function
	// Render ImGui
	commandList->BindRenderTarget(renderTarget);
	device->ImGuiRender(commandList);

	// Execute command list and display it
	device->ExecuteGraphicsCommandListAndFlip(commandList);
}

void Renderer::Present() 
{

}

void Renderer::ImGuiRender()
{
	static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None | ImGuiDockNodeFlags_PassthruCentralNode;
	static bool showImGui = false;

	ImGuiWindowFlags window_flags =   ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBackground
									| ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize 
									| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
	
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);
	
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.f);

	ImGui::Begin("DockSpace Demo", nullptr, window_flags);

	ImGui::PopStyleVar(4);

	ImGuiIO& io = ImGui::GetIO();
	if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
	{
		ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
		ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
	}

	ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.f);
	if (ImGui::BeginMenuBar())
	{
		ImGui::Checkbox("Show Imgui", &showImGui);

		ImGui::EndMenuBar();
	}
	ImGui::PopStyleVar(1);

	if (showImGui)
		ImGui::GetStyle().Alpha = .7f;
	else
		ImGui::GetStyle().Alpha = .0f;

	ImGui::End();

}

}