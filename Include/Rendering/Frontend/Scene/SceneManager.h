#pragma once

#include "Defines.h"

#include "Rendering/Frontend/Scene/Scene.h"

namespace Gecko {
	
	class ResourceManager;

	using SceneHandle = u32;
	class SceneManager
	{
	public:
		SceneManager() = default;
		~SceneManager() {};

		void Init();
		void Shutdown();

		[[nodiscard]] SceneHandle CreateScene(const std::string& name);

		u32 GetSceneCount() const;
		
		Scene* GetScene(SceneHandle handle) const;

	private:
		std::vector<Scope<Scene>> m_Scenes;
	};

}