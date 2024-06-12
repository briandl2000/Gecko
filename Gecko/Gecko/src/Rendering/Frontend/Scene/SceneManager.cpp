#include "Rendering/Frontend/Scene/SceneManager.h"

#include "Rendering/Frontend/Scene/Scene.h"
#include "Rendering/Frontend/Scene/GLTFSceneLoader.h"
#include "Rendering/Frontend/ResourceManager/ResourceManager.h"

#include <imgui.h>

namespace Gecko
{

void SceneManager::Init()
{
}

void SceneManager::Shutdown()
{
	m_Scenes.clear();
}

Ref<Scene> SceneManager::CreateScene()
{
	u32 index = static_cast<u32>(m_Scenes.size());
	m_Scenes.push_back(CreateRef<Scene>());

	Ref<Scene> scene = m_Scenes[index];
	scene->Init();

	return scene;
}

Ref<Scene> SceneManager::LoadGLTFScene(const std::string& pathString, ResourceManager* resourceManager)
{
	Ref<Scene> scene = CreateScene();

	GLTFSceneLoader::LoadScene(scene, pathString, resourceManager);

	return scene;
}

void SceneManager::ImGuiRender()
{
	//i32 SelectedNode = -1;

	ImGui::Begin("SceneManager");
	{
		for (u32 i = 0; i < m_Scenes.size(); i++)
		{
			//Ref<Scene>& scene = m_Scenes[i];

			//ImGui::TreeNodeEx();
		}
	}
}

}
