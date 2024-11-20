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

	Scope<SceneRenderObject> Scene::CreateSceneRenderObject() const
	{
		return CreateScopeFromRaw<SceneRenderObject>(new SceneRenderObject());
	}
	
	Scope<SceneLight> Scene::CreateLight(LightType lightType) const
	{
		switch (lightType)
		{
		case LightType::Directional:
			return CreateDirectionalLight();
		case LightType::Point:
			return CreatePointLight();
		case LightType::Spot:
			return CreateSpotLight();
		default:
			ASSERT(false, "Unknown light type!");
			return nullptr;
		}
	}

	Scope<SceneDirectionalLight> Scene::CreateDirectionalLight() const
	{
		return CreateScopeFromRaw<SceneDirectionalLight>(new SceneDirectionalLight());
	}
	
	Scope<ScenePointLight> Scene::CreatePointLight() const
	{
		return CreateScopeFromRaw<ScenePointLight>(new ScenePointLight());
	}

	Scope<SceneSpotLight> Scene::CreateSpotLight() const
	{
		return CreateScopeFromRaw<SceneSpotLight>(new SceneSpotLight());
	}

	Scope<SceneCamera> Scene::CreateCamera(ProjectionType type) const
	{
		return CreateScopeFromRaw<SceneCamera>(new SceneCamera(type));
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
