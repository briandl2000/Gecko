#include "Rendering/Frontend/Scene/SceneManager.h"

#include "Rendering/Frontend/Scene/Scene.h"
#include "Rendering/Frontend/ResourceManager/ResourceManager.h"

namespace Gecko {

	void SceneManager::Init()
	{

	}

	void SceneManager::Shutdown()
	{
		m_Scenes.clear();
	}

	SceneHandle SceneManager::CreateScene(const std::string& name)
	{
		m_Scenes.push_back(CreateScope<Scene>());
		m_Scenes.back()->Init(name);

		return static_cast<SceneHandle>(m_Scenes.size() - 1);
	}

	u32 SceneManager::GetSceneCount() const
	{
		return static_cast<u32>(m_Scenes.size());
	}

	Scene* SceneManager::GetScene(SceneHandle handle) const
	{
		if (handle >= GetSceneCount())
		{
			LOG_WARN("Scene index is greater then the scene count!");
			return nullptr;
		}

		return m_Scenes[handle].get();
	}
}
