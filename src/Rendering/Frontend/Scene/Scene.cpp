#include "Rendering/Frontend/Scene/Scene.h"

#include "Rendering/Frontend/Scene/SceneRenderInfo.h"

#include "Core/Platform.h"
#include "Core/Logger.h"

namespace Gecko  {

	void Scene::Init(const std::string& name)
	{
		AddEventListener(Event::SystemEvent::CODE_RESIZED, &Scene::OnResize);

		m_Name = name;
	}

	SceneRenderObject* Scene::CreateSceneRenderObject() const
	{
		return new SceneRenderObject();
	}
	
	SceneLight* Scene::CreateLight(LightType lightType) const
	{
		switch (lightType)
		{
		case LightType::Directional:
			return new SceneDirectionalLight();
		case LightType::Point:
			return new ScenePointLight();
		case LightType::Spot:
			return new SceneSpotLight();
		default:
			ASSERT_MSG(false, "Unkown light type!");
			return nullptr;
		}
	}

	SceneDirectionalLight* Scene::CreateDirectionalLight() const
	{
		return static_cast<SceneDirectionalLight*>(CreateLight(LightType::Directional));
	}
	
	ScenePointLight* Scene::CreatePointLight() const
	{
		return static_cast<ScenePointLight*>(CreateLight(LightType::Point));
	}

	SceneSpotLight* Scene::CreateSpotLight() const
	{
		return static_cast<SceneSpotLight*>(CreateLight(LightType::Spot));
	}

	SceneCamera* Scene::CreateCamera() const
	{
		return new SceneCamera();
	}

	SceneRenderInfo Scene::GetSceneRenderInfo() const
	{
		SceneRenderInfo sceneRenderInfo;

		glm::mat4 transform(1.f);

		PopulateSceneRenderInfo(&sceneRenderInfo, transform);

		return sceneRenderInfo;
	}

	EnvironmentMapHandle Scene::GetEnvironmentMapHandle() const
	{
		return m_EnvironmentMapHandle;
	}

	const std::string& Scene::GetName() const
	{
		return m_Name;
	}

	void Scene::SetEnvironmentMapHandle(EnvironmentMapHandle handle)
	{
		m_EnvironmentMapHandle = handle;
	}

#pragma warning(push)
#pragma warning(disable: 4100)
	const void Scene::PopulateSceneRenderInfo(SceneRenderInfo* sceneRenderInfo, const glm::mat4& transform) const
	{
		sceneRenderInfo->EnvironmentMap = m_EnvironmentMapHandle;
	}
#pragma warning(pop)
}
