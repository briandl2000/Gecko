#include "Rendering/Frontend/Scene/SceneManager.h"

namespace Gecko {

	void SceneManager::Init()
	{

	}

	void SceneManager::Shutdown()
	{
		m_Scenes.clear();
	}

	u32 SceneManager::GetSceneCount() const
	{
		return static_cast<u32>(m_Scenes.size());
	}
}
