#pragma once

#include "Defines.h"

#include "Rendering/Frontend/Scene/SceneObjects/SceneCamera.h"
#include "Rendering/Frontend/Scene/SceneObjects/SceneLight.h"
#include "Rendering/Frontend/Scene/SceneObjects/SceneRenderObject.h"
#include "Core/Event.h"

namespace Gecko {

	struct SceneRenderInfo;

	class Scene : Event::EventListener<Scene>
	{
	public:
		Scene() = default;
		virtual ~Scene() {};

		// Sets the name of this scene and initialises event listening. Should be called right after scene creation
		virtual void Init(const std::string& name);

		// Allocates requested object on the heap and returns pointer to it
		[[nodiscard]] virtual SceneRenderObject* CreateSceneRenderObject() const;
		// Allocates requested object on the heap and returns pointer to it
		[[nodiscard]] virtual SceneCamera* CreateCamera() const;
		// Allocates requested object on the heap and returns pointer to it
		[[nodiscard]] virtual SceneLight* CreateLight(LightType lightType) const;
		// Allocates requested object on the heap and returns pointer to it
		[[nodiscard]] SceneDirectionalLight* CreateDirectionalLight() const;
		// Allocates requested object on the heap and returns pointer to it
		[[nodiscard]] ScenePointLight* CreatePointLight() const;
		// Allocates requested object on the heap and returns pointer to it
		[[nodiscard]] SceneSpotLight* CreateSpotLight() const;
		
		// Returns a newly created SceneRenderInfo object filled with data from this scene
		[[nodiscard]] SceneRenderInfo GetSceneRenderInfo() const;

		EnvironmentMapHandle GetEnvironmentMapHandle() const;
		void SetEnvironmentMapHandle(EnvironmentMapHandle handle);

		const std::string& GetName() const;

		// Adjust aspect ratios of cameras
		virtual bool Scene::OnResize(const Event::EventData& data) = 0;

	protected:
		// Read data from this scene into a SceneRenderInfo object (how scene data is translated into the object should be defined
		// by derived classes based on their internal structure).
		// Don't forget to call this parent function if you want the environment map handle to be put in.
		virtual const void PopulateSceneRenderInfo(SceneRenderInfo* sceneRenderInfo, const glm::mat4& transform) const = 0;
	
	protected:
		EnvironmentMapHandle m_EnvironmentMapHandle{ 0 };
	
	private:
		std::string m_Name{ "Scene" };
	};

}