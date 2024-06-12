#include "Defines.h"

#include "Rendering/Frontend/Context.h"
#include "Rendering/Frontend/Scene/Scene.h"

#include "Rendering/Frontend/Renderer/RenderPasses/ShadowPass.h"
#include "Rendering/Frontend/Renderer/RenderPasses/ShadowRaytracePass.h"
#include "Rendering/Frontend/Renderer/RenderPasses/GeometryPass.h"
#include "Rendering/Frontend/Renderer/RenderPasses/DeferredPBRPass.h"
#include "Rendering/Frontend/Renderer/RenderPasses/FXAAPass.h"
#include "Rendering/Frontend/Renderer/RenderPasses/BloomPass.h"
#include "Rendering/Frontend/Renderer/RenderPasses/ToneMappingGammaCorrectionPass.h"

#include "glm/gtx/quaternion.hpp"
#include <imgui.h>

int main()
{

	Gecko::Platform::AppInfo info;
	info.Width = 900;
	info.Height = 600;
	info.FullScreenWidth = 1920;
	info.FullScreenHeight = 1080;
	info.X = 200;
	info.Y = 200;
	info.NumBackBuffers = 2;
	info.Name = "Gecko App";

	Gecko::Platform::Init(info);

	Gecko::Logger::Init();

	// Create the context
	Gecko::Context ctx;
	ctx.Init(info);

	Gecko::Renderer* renderer = ctx.GetRenderer();
	Gecko::ResourceManager* resourceManager = ctx.GetResourceManager();
	Gecko::SceneManager* sceneManager = ctx.GetSceneManager();

	// Create the render passess
	Gecko::Ref<Gecko::ShadowPass> shadowPass = renderer->CreateRenderPass<Gecko::ShadowPass>();
	Gecko::Ref<Gecko::GeometryPass> geometryPass = renderer->CreateRenderPass<Gecko::GeometryPass>();
	Gecko::Ref<Gecko::DeferredPBRPass> deferredPBRPass = renderer->CreateRenderPass<Gecko::DeferredPBRPass>();
	Gecko::Ref<Gecko::FXAAPass> FXAAPass = renderer->CreateRenderPass<Gecko::FXAAPass>();
	Gecko::Ref<Gecko::BloomPass> bloomPass= renderer->CreateRenderPass<Gecko::BloomPass>();
	Gecko::Ref<Gecko::ToneMappingGammaCorrectionPass> toneMappingGammaCorrectionPass = renderer->CreateRenderPass<Gecko::ToneMappingGammaCorrectionPass>();
	
	// Configure renderpasses
	renderer->ConfigureRenderPasses(
		{
			shadowPass,
			geometryPass,
			deferredPBRPass,
			FXAAPass,
			bloomPass,
			toneMappingGammaCorrectionPass
		}
	);

	// Create a scene
	Gecko::Ref<Gecko::Scene> scene = sceneManager->CreateScene();
	scene->EnvironmentMapHandle = resourceManager->CreateEnvironmentMap("Assets/scythian_tombs_2_4k.hdr");

	// Load the Sponza gltf scene
	Gecko::Ref<Gecko::Scene> sponzaScene = sceneManager->LoadGLTFScene("Assets/sponza/glTF/Sponza.gltf", resourceManager);
	Gecko::Ref<Gecko::SceneNode> sponzaNode = scene->GetRootNode()->AddNode();
	sponzaNode->AddScene(sponzaScene);
	sponzaNode->Transform.Rotation.y = glm::radians(90.f);

	// Load Helmet gltf Scene
	Gecko::Ref<Gecko::Scene> helmetScene = sceneManager->LoadGLTFScene("Assets/gltfHelmet/glTF/DamagedHelmet.gltf", resourceManager);
	Gecko::Ref<Gecko::SceneNode> helmetRootNode = scene->GetRootNode()->AddNode();
	helmetRootNode->AddScene(helmetScene);
	helmetRootNode->Transform.Position.y = 3.f;

	// Create a camera in the scene
	Gecko::Ref<Gecko::SceneNode> cameraNode = scene->GetRootNode()->AddNode();
	cameraNode->AddCamera(90.f);
	cameraNode->Transform.Position.z = 4.f;
	cameraNode->Transform.Position.y = 2.f;

	Gecko::f32 lastTime = Gecko::Platform::GetTime();

	while (Gecko::Platform::IsRunning()) {
		Gecko::Platform::PumpMessage();

		Gecko::f32 currentTime = Gecko::Platform::GetTime();
		Gecko::f32 deltaTime = (currentTime - lastTime);
		lastTime = currentTime;

		helmetRootNode->Transform.Rotation.y += .73f * deltaTime;
		helmetRootNode->Transform.Rotation.x += 1.6f * deltaTime;
		helmetRootNode->Transform.Rotation.z += 1.0f * deltaTime;
	
		cameraNode->Transform.Rotation += Gecko::Platform::GetRotationInput() * deltaTime;

		glm::vec3 movement = glm::toMat3(glm::quat(cameraNode->Transform.Rotation)) * Gecko::Platform::GetPositionInput();
		cameraNode->Transform.Position +=  movement * deltaTime;

		// Do the imgui things
		renderer->ImGuiRender();
		ImGui::ShowDemoWindow();
		scene->ImGuiRender();

		Gecko::SceneDescriptor sceneDescriptor = scene->GetSceneDescriptor();
		renderer->RenderScene(sceneDescriptor);
	}

	ctx.Shutdown();

	Gecko::Logger::Shutdown();

	Gecko::Platform::Shutdown();

	return 0;
}