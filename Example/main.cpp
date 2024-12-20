#include "Defines.h"

#include "Rendering/Frontend/ApplicationContext.h"

#include "GLTFSceneLoader.h"

#include "Rendering/Frontend/Renderer/RenderPasses/RenderPass.h"
#include "Rendering/Frontend/Renderer/RenderPasses/ShadowPass.h"
#include "Rendering/Frontend/Renderer/RenderPasses/GeometryPass.h"
#include "Rendering/Frontend/Renderer/RenderPasses/DeferredPBRPass.h"
#include "Rendering/Frontend/Renderer/RenderPasses/FXAAPass.h"
#include "Rendering/Frontend/Renderer/RenderPasses/BloomPass.h"
#include "Rendering/Frontend/Renderer/RenderPasses/ToneMappingGammaCorrectionPass.h"
#include "Rendering/Frontend/Scene/SceneObjects/Transform.h"

#include "Core/Input.h"

#include "ExampleComputePass.h"
#include "NodeBasedScene.h"
#include "DebugSceneUIRenderer.h"

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
	info.WorkingDir = WORKING_DIR_PATH;

	Gecko::Event::Init();
	Gecko::Platform::Init(info);
	Gecko::Input::Init();
	Gecko::Logger::Init();

	// Create the context
	Gecko::ApplicationContext ctx;
	ctx.Init(info);

	Gecko::Renderer* renderer = ctx.GetRenderer();
	Gecko::ResourceManager* resourceManager = ctx.GetResourceManager();

	// Create the render passess
	Gecko::RenderPassHandle shadowPass = renderer->CreateRenderPass<Gecko::ShadowPass>("Shadow");
	Gecko::RenderPassHandle geometryPass = renderer->CreateRenderPass<Gecko::GeometryPass>("Geo");

	Gecko::DeferredPBRPass::ConfigData PBRConfigData;
	PBRConfigData.GeoPass = geometryPass;
	PBRConfigData.ShadowPass = shadowPass;
	Gecko::RenderPassHandle deferredPBRPass = renderer->CreateRenderPass<Gecko::DeferredPBRPass>("PBR", PBRConfigData);

	Gecko::FXAAPass::ConfigData FXAAConfigData(deferredPBRPass);
	Gecko::RenderPassHandle FXAAPass = renderer->CreateRenderPass<Gecko::FXAAPass>("FXAA", FXAAConfigData);

	Gecko::BloomPass::ConfigData BloomConfigData(renderer->GetRenderPassByHandle(FXAAPass)->GetOutputHandle());
	Gecko::RenderPassHandle bloomPass = renderer->CreateRenderPass<Gecko::BloomPass>("Bloom", BloomConfigData);

	Gecko::ToneMappingGammaCorrectionPass::ConfigData ToneMappingGammaCorrectionConfigData;
	ToneMappingGammaCorrectionConfigData.PrevPassOutput = renderer->GetRenderPassByHandle(bloomPass)->GetOutputHandle();
	Gecko::RenderPassHandle toneMappingGammaCorrectionPass = 
		renderer->CreateRenderPass<Gecko::ToneMappingGammaCorrectionPass>("ToneMappingGammaCorrection",
			ToneMappingGammaCorrectionConfigData);

	//Gecko::RenderPassHandle exampleComputePass = renderer->CreateRenderPass<ExampleComputePass>("ExampleCompute");
	
	// Configure renderpasses
	renderer->ConfigureRenderPasses({
		shadowPass,
		geometryPass,
		deferredPBRPass,
		FXAAPass,
		bloomPass,
		toneMappingGammaCorrectionPass,
		//exampleComputePass
		});

	// Main scene initialisation
	Gecko::SceneManager* sceneManager = ctx.GetSceneManager();
	Gecko::u32 mainSceneIdx = sceneManager->CreateScene<NodeBasedScene>("Main Scene");
	NodeBasedScene* mainScene = sceneManager->GetScene<NodeBasedScene>(mainSceneIdx);

	// Add an environment map
	mainScene->SetEnvironmentMapHandle(resourceManager->CreateEnvironmentMap("Assets/scythian_tombs_2_4k.hdr"));

	// Load the Sponza gltf scene
	SceneNode* sponzaNode = mainScene->GetRootNode()->AddNode("Sponza node");
	Gecko::SceneHandle sponzaScene = GLTFSceneLoader::LoadNodeBasedScene("Assets/sponza/glb/Sponza.glb", ctx);
	sponzaNode->AppendSceneData(*sceneManager->GetScene<NodeBasedScene>(sponzaScene));
	sponzaNode->GetModifiableTransform().Rotation.y = 90.f;

	// Load Helmet gltf Scene
	SceneNode* helmetRootNode = mainScene->GetRootNode()->AddNode("Helmet node");
	Gecko::SceneHandle helmetScene = GLTFSceneLoader::LoadNodeBasedScene("Assets/gltfHelmet/glTF-Binary/DamagedHelmet.glb", ctx);
	helmetRootNode->AppendSceneData(*sceneManager->GetScene<NodeBasedScene>(helmetScene));
	helmetRootNode->GetModifiableTransform().Position.y = 3.f;

	// Create a camera in the scene
	SceneNode* cameraNode = mainScene->GetRootNode()->AddNode("Camera node");
	Gecko::Scope<Gecko::SceneCamera> camera = mainScene->CreateCamera(Gecko::ProjectionType::Perspective);
	camera->SetIsMain(true);
	camera->SetAutoAspectRatio(true);
	cameraNode->AttachCamera(&camera);
	cameraNode->GetModifiableTransform().Position.z = 4.f;
	cameraNode->GetModifiableTransform().Position.y = 2.f;

	// Create directional light
	Gecko::Scope<Gecko::SceneDirectionalLight> directionalLight = mainScene->CreateDirectionalLight();
	SceneNode* lightNode = mainScene->GetRootNode()->AddNode("Light node");
	directionalLight->SetColor({ 1., 1., 1. });
	directionalLight->SetIntenstiy(1.f);
	lightNode->AppendLight(&Gecko::CreateScopeFromRaw<Gecko::SceneLight>(directionalLight.release()));
	lightNode->GetModifiableTransform().Rotation.x = -90.f;

	Gecko::f32 lastTime = Gecko::Platform::GetTime();
	

	while (Gecko::Platform::IsRunning()) {
		Gecko::Platform::PumpMessage();

		Gecko::f32 currentTime = Gecko::Platform::GetTime();
		Gecko::f32 deltaTime = (currentTime - lastTime);
		lastTime = currentTime;

		helmetRootNode->GetModifiableTransform().Rotation.y += .73f * deltaTime * 50.f;
		helmetRootNode->GetModifiableTransform().Rotation.x += 1.6f * deltaTime * 50.f;
		helmetRootNode->GetModifiableTransform().Rotation.z += 1.0f * deltaTime * 50.f;
	
		glm::vec3 rot{ 0. };
		
		if (Gecko::Input::IsKeyDown(Gecko::Input::Key::UP))		rot.x += 1.f;
		if (Gecko::Input::IsKeyDown(Gecko::Input::Key::DOWN))	rot.x -= 1.f;
		if (Gecko::Input::IsKeyDown(Gecko::Input::Key::LEFT))	rot.y += 1.f;
		if (Gecko::Input::IsKeyDown(Gecko::Input::Key::RIGHT))	rot.y -= 1.f;

		cameraNode->GetModifiableTransform().Rotation += rot * deltaTime * 40.f;
		
		glm::vec3 pos{ 0. };

		if (Gecko::Input::IsKeyDown(Gecko::Input::Key::W))		pos.z -= 1.f;
		if (Gecko::Input::IsKeyDown(Gecko::Input::Key::S))		pos.z += 1.f;
		if (Gecko::Input::IsKeyDown(Gecko::Input::Key::A))		pos.x -= 1.f;
		if (Gecko::Input::IsKeyDown(Gecko::Input::Key::D))		pos.x += 1.f;
		if (Gecko::Input::IsKeyDown(Gecko::Input::Key::LSHIFT)) pos.y -= 1.f;
		if (Gecko::Input::IsKeyDown(Gecko::Input::Key::SPACE))	pos.y += 1.f;
		if (glm::length2(pos) > 0.) pos = glm::normalize(pos);

		glm::vec3 movement = glm::mat3(cameraNode->GetModifiableTransform().GetMat4()) * pos;
		
		cameraNode->GetModifiableTransform().Position += movement * deltaTime;

		// Do the imgui things
		DebugSceneUIRenderer::RenderDebugUI(ctx);
		
		renderer->RenderScene(mainScene->GetSceneRenderInfo());


		Gecko::Input::Update();
	}

	ctx.Shutdown();

	Gecko::Logger::Shutdown();
	Gecko::Input::Shutdown();
	Gecko::Platform::Shutdown();
	Gecko::Event::Shutdown();

	return 0;
}