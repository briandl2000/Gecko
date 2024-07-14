#include "Rendering/Frontend/Scene/SceneManager.h"

#include "Rendering/Frontend/Scene/Scene.h"
#include "Rendering/Frontend/ResourceManager/ResourceManager.h"

namespace Gecko {

	void SceneManager::Init()
	{
		Platform::AddResizeEvent(SceneManager::OnResize, this);
	}

	void SceneManager::Shutdown()
	{
		m_Scenes.clear();
	}

	Scene* SceneManager::CreateScene(const std::string& name)
	{
		u32 index = static_cast<u32>(m_Scenes.size());
		m_Scenes.push_back(CreateScope<Scene>());

		Scene* scene = m_Scenes[index].get();
		scene->Init(name);

		return scene;
	}

	u32 SceneManager::GetSceneCount() const
	{
		return static_cast<u32>(m_Scenes.size());
	}

	Scene* SceneManager::GetScene(u32 sceneIndex) const
	{
		if (sceneIndex >= GetSceneCount())
		{
			LOG_WARN("Scene index is greater then the scene count!");
			return nullptr;
		}

		return m_Scenes[sceneIndex].get();
	}

	void SceneManager::OnResize(u32 width, u32 height, void* listener)
	{
		SceneManager* sceneManager = reinterpret_cast<SceneManager*>(listener);
		
		for (u32 i = 0; i < sceneManager->m_Scenes.size(); i++)
		{
			sceneManager->m_Scenes[i]->OnResize(width, height);
		}
	}

}
