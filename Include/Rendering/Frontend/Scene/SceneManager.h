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

		// Create an empty scene and return a handle to it; T should be the scene class or a derived class
		template<typename T>
		[[nodiscard]] SceneHandle CreateScene(const std::string& name)
		{
			m_Scenes.push_back(CreateScope<T>());
			m_Scenes.back()->Init(name);

			return static_cast<SceneHandle>(m_Scenes.size() - 1);
		}

		// Get the number of scenes contained in this scene manager
		u32 GetSceneCount() const;
		
		// Get a scene by handle; T should be the Scene class or a derived class
		template <typename T>
		[[nodiscard]] T* GetScene(SceneHandle handle) const
		{
			if (handle >= GetSceneCount())
			{
				LOG_WARN("Scene index is greater then the scene count!");
				return nullptr;
			}

			return static_cast<T*>(m_Scenes[handle].get());
		}

	private:
		std::vector<Scope<Scene>> m_Scenes;
	};

}