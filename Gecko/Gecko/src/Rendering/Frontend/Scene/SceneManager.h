#pragma once

#include "Defines.h"

#include "Rendering/Frontend/Scene/Scene.h"

namespace Gecko {
	
	class ResourceManager;

	class SceneManager
	{
	public:
		SceneManager() = default;
		~SceneManager() {};

		void Init();
		void Shutdown();

		[[nodiscard]] Scene* CreateScene(const std::string& name);

		u32 GetSceneCount() const;
		
		Scene* GetScene(u32 sceneIndex) const;

		static void OnResize(u32 width, u32 height, void* listener);

	private:
		std::vector<Scope<Scene>> m_Scenes;
	};

}