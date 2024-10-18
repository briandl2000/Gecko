#include "Rendering/Frontend/Scene/Scene.h"

#include "Rendering/Frontend/Scene/SceneRenderInfo.h"

#include "Platform/Platform.h"

namespace Gecko  {

	SceneNode* SceneNode::AddNode(const std::string& name)
	{
		u32 index = static_cast<u32>(m_Children.size());
		m_Children.push_back(CreateScope<SceneNode>());

		SceneNode* node = m_Children[index].get();
		node->SetName(name);
		return node;
	}

	void SceneNode::AppendSceneRenderObject(SceneRenderObject* sceneRenderObject)
	{
		m_SceneRenderObjects.push_back(sceneRenderObject);
	}

	void SceneNode::AppendLight(SceneLight* light)
	{
		m_Lights.push_back(light);
	}

	void SceneNode::AttachCamera(SceneCamera* camera)
	{
		m_Camera = camera;
	}

	void SceneNode::AppendScene(Scene* scene)
	{
		m_Scenes.push_back(scene);
	}

	u32 SceneNode::GetChildrenCount()
	{
		return static_cast<u32>(m_Children.size());
	}

	SceneNode* SceneNode::GetChild(u32 childNodeIndex)
	{
		if (childNodeIndex >= GetChildrenCount())
		{
			LOG_WARN("Node index is greater than the children count!");
			return nullptr;
		}
		return m_Children[childNodeIndex].get();
	}

	const void SceneNode::SetName(const std::string& name)
	{
		m_Name = name;
	}

	const std::string& SceneNode::GetName()
	{
		return m_Name;
	}

	const void SceneNode::PopulateSceneRenderInfo(SceneRenderInfo& sceneRenderInfo, glm::mat4 transform) const
	{
	
		// Calculate the transform of this nodes
		glm::mat4 transformMatrix = Transform.GetMat4();
		glm::mat4 worldMatrix = transform * transformMatrix;

		// Add the meshes
		for (const SceneRenderObject* sceneRenderObject : m_SceneRenderObjects)
		{
			sceneRenderInfo.RenderObjects.push_back({
				sceneRenderObject->GetMeshHandle(),
				sceneRenderObject->GetMaterialHandle(),
				worldMatrix}
			);
		}

		// Add the camera
		if (m_Camera != nullptr && !sceneRenderInfo.HasCamera)
		{
			glm::vec3 right = glm::normalize(glm::mat3(worldMatrix) * glm::vec3(1.f, 0.f, 0.f));
			glm::vec3 up = glm::normalize(glm::mat3(worldMatrix) * glm::vec3(0.f, 1.f, 0.f));
			glm::vec3 forward = glm::normalize(glm::mat3(worldMatrix) * glm::vec3(0.f, 0.f, 1.f));

			glm::vec3 position = glm::vec3(worldMatrix * glm::vec4(0.f, 0.f, 0.f, 1.f));

			glm::mat4 view = glm::mat4(
				glm::vec4(right, 0.f),
				glm::vec4(up, 0.f),
				glm::vec4(forward, 0.f),
				glm::vec4(position, 1.f)
			);
			sceneRenderInfo.Camera.View = view;
			sceneRenderInfo.Camera.Projection = m_Camera->GetCachedProjection();
			sceneRenderInfo.HasCamera = true;
		}

		// Add the lights
		for (const SceneLight* sceneLight : m_Lights)
		{
			switch (sceneLight->GetLightType())
			{
			case LightType::Directional:
				{
					const SceneDirectionalLight* directionalLight = reinterpret_cast<const SceneDirectionalLight*>(sceneLight);
					sceneRenderInfo.DirectionalLights.push_back({
						directionalLight->GetColor(),
						directionalLight->GetIntenstiy(),
						worldMatrix
					});
				}
				break;
			case LightType::Point:
				{
					const ScenePointLight* pointLight = reinterpret_cast<const ScenePointLight*>(sceneLight);
					sceneRenderInfo.PointLights.push_back({
						pointLight->GetColor(),
						pointLight->GetIntenstiy(),
						worldMatrix,
						pointLight->GetRadius()
						});
				}
				break;
			case LightType::Spot:
				{
					const SceneSpotLight* spotLight = reinterpret_cast<const SceneSpotLight*>(sceneLight);
					sceneRenderInfo.SpotLights.push_back({
						spotLight->GetColor(),
						spotLight->GetIntenstiy(),
						worldMatrix,
						spotLight->GetRadius(),
						spotLight->GetAngle()
						});
				}
				break;
			case LightType::None:
				LOG_WARN("Invalid light type None!");
				break;
			default:
				LOG_WARN("Unkown light type: %ui!", static_cast<u32>(sceneLight->GetLightType()));
				break;
			}
		}

		// Add the children
		for (const Scope<SceneNode>& child : m_Children)
		{
			child->PopulateSceneRenderInfo(sceneRenderInfo, worldMatrix);
		}

		// Append the scene
		for (const Scene* scene : m_Scenes)
		{
			scene->PopulateSceneRenderInfo(sceneRenderInfo, worldMatrix);
		}
	}

	void Scene::Init(const std::string& name)
	{
		m_Name = name;
		m_RootNode = CreateScope<SceneNode>();
		m_RootNode->SetName("Root Node");
	}

	SceneNode* Scene::GetRootNode()
	{
		return m_RootNode.get();
	}

	SceneRenderObject* Scene::CreateSceneRenderObject()
	{
		u32 index = static_cast<u32>(m_SceneRenderObject.size());
		m_SceneRenderObject.push_back(CreateScope<SceneRenderObject>());

		SceneRenderObject* sceneRenderObject = m_SceneRenderObject[index].get();
		return sceneRenderObject;
	}
	
	SceneLight* Scene::CreateLight(LightType lightType)
	{
		u32 index = static_cast<u32>(m_Lights.size());
	
		switch (lightType)
		{
		case LightType::Directional:
			m_Lights.push_back(CreateScope<SceneDirectionalLight>());
			break;
		case LightType::Point:
			m_Lights.push_back(CreateScope<ScenePointLight>());
			break;
		case LightType::Spot:
			m_Lights.push_back(CreateScope<SceneSpotLight>());
			break;
		default:
			ASSERT_MSG(false, "Unkown light type!");
			return nullptr;
			break;
		}

		SceneLight* light = m_Lights[index].get();
		return light;
	}

	SceneDirectionalLight* Scene::CreateDirectionalLight()
	{
		return reinterpret_cast<SceneDirectionalLight*>(CreateLight(LightType::Directional));
	}
	
	ScenePointLight* Scene::CreatePointLight()
	{
		return reinterpret_cast<ScenePointLight*>(CreateLight(LightType::Point));
	}

	SceneSpotLight* Scene::CreateSpotLight()
	{
		return reinterpret_cast<SceneSpotLight*>(CreateLight(LightType::Spot));
	}

	SceneCamera* Scene::CreateCamera()
	{
		u32 index = static_cast<u32>(m_Cameras.size());
		m_Cameras.push_back(CreateScope<SceneCamera>());

		SceneCamera* camera = m_Cameras[index].get();
		return camera;
	}

	const SceneRenderInfo Scene::GetSceneRenderInfo() const
	{
		SceneRenderInfo sceneRenderInfo;

		glm::mat4 transform(1.f);

		PopulateSceneRenderInfo(sceneRenderInfo, transform);

		return sceneRenderInfo;
	}

	EnvironmentMapHandle Scene::GetEnvironmentMapHandle() const
	{
		return m_EnvironmentMapHandle;
	}

	void Scene::SetEnvironmentMapHandle(EnvironmentMapHandle handle)
	{
		m_EnvironmentMapHandle = handle;
	}

	void Scene::OnResize(u32 width, u32 height)
	{
		f32 aspectRatio = static_cast<f32>(width) / static_cast<f32>(height);
		for (u32 i = 0; i < m_Cameras.size(); i++)
		{
			if (m_Cameras[i]->IsAutoAspectRatio())
			{
				m_Cameras[i]->SetAspectRatio(aspectRatio);
			}
		}
	}

	const void Scene::PopulateSceneRenderInfo(SceneRenderInfo& sceneRenderInfo, glm::mat4 transform) const
	{
		sceneRenderInfo.EnvironmentMap = m_EnvironmentMapHandle;

		m_RootNode->PopulateSceneRenderInfo(sceneRenderInfo, transform);
	}

}