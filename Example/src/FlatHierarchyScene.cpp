#include "FlatHierarchyScene.h"

#include "Rendering/Frontend/Scene/SceneRenderInfo.h"

#include "Core/Platform.h"
#include "Core/Logger.h"

#pragma region typenames
using SceneRenderObject = Gecko::SceneRenderObject;
using SceneLight = Gecko::SceneLight;
using SceneDirectionalLight = Gecko::SceneDirectionalLight;
using ScenePointLight = Gecko::ScenePointLight;
using SceneSpotLight = Gecko::SceneSpotLight;
using LightType = Gecko::LightType;
using SceneCamera = Gecko::SceneCamera;
using ProjectionType = Gecko::ProjectionType;
using SceneRenderInfo = Gecko::SceneRenderInfo;
#pragma endregion

void FlatHierarchyScene::Init(const std::string& name)
{
	Gecko::Scene::Init(name);
}

const void FlatHierarchyScene::PopulateSceneRenderInfo(SceneRenderInfo* sceneRenderInfo, const glm::mat4& transform) const
{
	// Add the camera
	if (m_MainCamera != nullptr && !sceneRenderInfo->HasCamera)
	{
		const glm::mat4& cameraTransform = m_MainCamera->GetTransform().GetMat4() * transform;
		glm::mat3 cameraRot = glm::mat3(cameraTransform);
		glm::vec3 right = glm::normalize(cameraRot * glm::vec3(1.f, 0.f, 0.f));
		glm::vec3 up = glm::normalize(cameraRot * glm::vec3(0.f, 1.f, 0.f));
		glm::vec3 forward = glm::normalize(cameraRot * glm::vec3(0.f, 0.f, 1.f));

		glm::vec3 position = glm::vec3(cameraTransform * glm::vec4(0.f, 0.f, 0.f, 1.f));

		glm::mat4 view = glm::mat4(
			glm::vec4(right, 0.f),
			glm::vec4(up, 0.f),
			glm::vec4(forward, 0.f),
			glm::vec4(position, 1.f)
		);
		sceneRenderInfo->Camera.View = view;
		sceneRenderInfo->Camera.Projection = m_MainCamera->GetCachedProjection();
		sceneRenderInfo->HasCamera = true;
	}

	// Add the meshes
	for (const Gecko::Scope<SceneRenderObject>& sceneRenderObject : m_SceneRenderObjects)
	{
		sceneRenderInfo->RenderObjects.push_back({
			sceneRenderObject->GetMeshHandle(),
			sceneRenderObject->GetMaterialHandle(),
			sceneRenderObject->GetTransform().GetMat4() * transform
			});
	}

	// Add the lights
	for (const Gecko::Scope<SceneLight>& sceneLight : m_Lights)
	{
		switch (sceneLight->GetLightType())
		{
		case LightType::Directional:
		{
			const SceneDirectionalLight& directionalLight = static_cast<const SceneDirectionalLight&>(*sceneLight);
			sceneRenderInfo->DirectionalLights.push_back({
				directionalLight.GetColor(),
				directionalLight.GetIntenstiy(),
				sceneLight->GetTransform().GetMat4() * transform
				});
		}
		break;
		case LightType::Point:
		{
			const ScenePointLight& pointLight = static_cast<const ScenePointLight&>(*sceneLight);
			sceneRenderInfo->PointLights.push_back({
				pointLight.GetColor(),
				pointLight.GetIntenstiy(),
				sceneLight->GetTransform().GetMat4() * transform,
				pointLight.GetRadius()
				});
		}
		break;
		case LightType::Spot:
		{
			const SceneSpotLight& spotLight = static_cast<const SceneSpotLight&>(*sceneLight);
			sceneRenderInfo->SpotLights.push_back({
				spotLight.GetColor(),
				spotLight.GetIntenstiy(),
				sceneLight->GetTransform().GetMat4() * transform,
				spotLight.GetRadius(),
				spotLight.GetAngle()
				});
		}
		break;
		case LightType::None:
			LOG_WARN("Invalid light type None!");
			break;
		default:
			LOG_WARN("Unkown light type: %ui!", static_cast<Gecko::u32>(sceneLight->GetLightType()));
			break;
		}
	}
}

bool FlatHierarchyScene::OnResize(const Gecko::Event::EventData& data)
{
	bool resized = false;
	if (m_MainCamera)
	{
		if (m_MainCamera->IsAutoAspectRatio())
		{
			Gecko::u32 width = data.Data.u32[0];
			Gecko::u32 height = data.Data.u32[1];
			width = width < 1 ? 1 : width;
			height = height < 1 ? 1 : height;
			Gecko::f32 aspectRatio = static_cast<Gecko::f32>(width) / static_cast<Gecko::f32>(height);
			m_MainCamera->SetAspectRatio(aspectRatio);
			resized = true;
		}
	}
	for (const Gecko::Scope<SceneCamera>& cam : m_AdditionalCameras)
	{
		if (cam->IsAutoAspectRatio())
		{
			Gecko::u32 width = data.Data.u32[0];
			Gecko::u32 height = data.Data.u32[1];
			width = width < 1 ? 1 : width;
			height = height < 1 ? 1 : height;
			Gecko::f32 aspectRatio = static_cast<Gecko::f32>(width) / static_cast<Gecko::f32>(height);
			cam->SetAspectRatio(aspectRatio);
			resized = true;
		}
	}
	return resized;
}

void FlatHierarchyScene::AppendSceneRenderObject(Gecko::Scope<Gecko::SceneRenderObject>* object)
{
	m_SceneRenderObjects.push_back(std::move(*object));
}

void FlatHierarchyScene::AppendLight(Gecko::Scope<Gecko::SceneLight>* light)
{
	m_Lights.push_back(std::move(*light));
}

void FlatHierarchyScene::AttachMainCamera(Gecko::Scope<Gecko::SceneCamera>* camera)
{
	m_MainCamera = std::move(*camera);
}

void FlatHierarchyScene::AppendAdditionalCamera(Gecko::Scope<Gecko::SceneCamera>* camera)
{
	m_AdditionalCameras.push_back(std::move(*camera));
}

void FlatHierarchyScene::AppendSceneData(FlatHierarchyScene* otherScene)
{
	for (Gecko::Scope<SceneRenderObject>& obj : otherScene->m_SceneRenderObjects)
	{
		m_SceneRenderObjects.push_back(std::move(obj));
	}
	otherScene->m_SceneRenderObjects.clear();

	for (Gecko::Scope<SceneLight>& light : otherScene->m_Lights)
	{
		m_Lights.push_back(std::move(light));
	}
	otherScene->m_Lights.clear();

	if (!m_MainCamera)
	{
		m_MainCamera = std::move(otherScene->m_MainCamera);
	}
	else
	{
		m_AdditionalCameras.push_back(std::move(otherScene->m_MainCamera));
	}

	for (Gecko::Scope<SceneCamera>& cam : otherScene->m_AdditionalCameras)
	{
		m_AdditionalCameras.push_back(std::move(cam));
	}
	otherScene->m_AdditionalCameras.clear();
}
