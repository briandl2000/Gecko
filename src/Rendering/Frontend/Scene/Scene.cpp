#include "Rendering/Frontend/Scene/Scene.h"

#include "Rendering/Frontend/Scene/SceneRenderInfo.h"

#include "Core/Platform.h"
#include "Core/Logger.h"

namespace Gecko  {

	SceneNode* SceneNode::AddNode(const std::string& name)
	{
		u32 index = static_cast<u32>(m_Children.size());
		m_Children.push_back(CreateScope<SceneNode>());

		SceneNode* node = m_Children[index].get();
		node->SetName(name);
		return node;
	}

	void SceneNode::AppendSceneRenderObject(Scope<SceneRenderObject>* sceneRenderObject)
	{
		m_SceneRenderObjects.push_back(std::move(*sceneRenderObject));
	}

	void SceneNode::AppendLight(Scope<SceneLight>* light)
	{
		m_Lights.push_back(std::move(*light));
	}

	void SceneNode::AttachCamera(Scope<SceneCamera>* camera)
	{
		m_Camera = std::move(*camera);
	}

	void SceneNode::AppendSceneData(const Scene& scene)
	{
		m_Children.push_back(scene.CopySceneToNode());
	}

	void SceneNode::RecursiveCopy(SceneNode* target) const
	{
		target->Transform = Transform;

		for (const Scope<SceneRenderObject>& obj : m_SceneRenderObjects)
		{ // Construct new objects with new Scopes pointing to them, then pass those to the target node
			SceneRenderObject* pObj = new SceneRenderObject(*obj.get());
			Scope<SceneRenderObject> sObj = CreateScopeFromRaw<SceneRenderObject>(pObj);
			target->m_SceneRenderObjects.push_back(std::move(sObj));
		}

		for (const Scope<SceneLight>& light : m_Lights)
		{ // Construct new lights with new Scopes pointing to them, then pass those to the target node
			SceneLight* pLight = new SceneLight(*light.get());
			Scope<SceneLight> sLight = CreateScopeFromRaw<SceneLight>(pLight);
			target->m_Lights.push_back(std::move(sLight));
		}

		if (m_Camera)
		{ // Construct a new camera with a new Scope pointing to it, then pass that to the target node
			SceneCamera* pCam = new SceneCamera(*m_Camera.get());
			Scope<SceneCamera> sCam = CreateScopeFromRaw<SceneCamera>(pCam);
			target->AttachCamera(&sCam);
		}

		for (const Scope<SceneNode>& c : m_Children)
		{
			SceneNode* n = target->AddNode(c->GetName());
			c->RecursiveCopy(n);
		}
	}

	u32 SceneNode::GetChildrenCount() const
	{
		return static_cast<u32>(m_Children.size());
	}

	SceneNode* SceneNode::GetChild(u32 childNodeIndex) const
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

	const std::string& SceneNode::GetName() const
	{
		return m_Name;
	}

	const void SceneNode::PopulateSceneRenderInfo(SceneRenderInfo* sceneRenderInfo, const glm::mat4& transform) const
	{
	
		// Calculate the transform of this node
		glm::mat4 transformMatrix = Transform.GetMat4();
		glm::mat4 worldMatrix = transform * transformMatrix;

		// Add the meshes
		for (const Scope<SceneRenderObject>& sceneRenderObject : m_SceneRenderObjects)
		{
			sceneRenderInfo->RenderObjects.push_back({
				sceneRenderObject->GetMeshHandle(),
				sceneRenderObject->GetMaterialHandle(),
				worldMatrix}
			);
		}

		// Add the camera
		if (m_Camera != nullptr && !sceneRenderInfo->HasCamera)
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
			sceneRenderInfo->Camera.View = view;
			sceneRenderInfo->Camera.Projection = m_Camera->GetCachedProjection();
			sceneRenderInfo->HasCamera = true;
		}

		// Add the lights
		for (const Scope<SceneLight>& sceneLight : m_Lights)
		{
			switch (sceneLight->GetLightType())
			{
			case LightType::Directional:
				{
					const SceneDirectionalLight& directionalLight = static_cast<const SceneDirectionalLight&>(*sceneLight);
					sceneRenderInfo->DirectionalLights.push_back({
						directionalLight.GetColor(),
						directionalLight.GetIntenstiy(),
						worldMatrix
					});
				}
				break;
			case LightType::Point:
				{
					const ScenePointLight& pointLight = static_cast<const ScenePointLight&>(*sceneLight);
					sceneRenderInfo->PointLights.push_back({
						pointLight.GetColor(),
						pointLight.GetIntenstiy(),
						worldMatrix,
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
						worldMatrix,
						spotLight.GetRadius(),
						spotLight.GetAngle()
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
	}

	void Scene::Init(const std::string& name)
	{
		AddEventListener(Event::SystemEvent::CODE_RESIZED, &Scene::OnResize);

		m_Name = name;
		m_RootNode = CreateScope<SceneNode>();
		m_RootNode->SetName("Root Node");
	}

	SceneNode* Scene::GetRootNode() const
	{
		return m_RootNode.get();
	}

	Scope<SceneNode> Scene::CopySceneToNode() const
	{
		Scope<SceneNode> newNode = CreateScope<SceneNode>();
		newNode->SetName(m_RootNode->GetName());
		m_RootNode->RecursiveCopy(newNode.get());
		return newNode;
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

	const SceneRenderInfo Scene::GetSceneRenderInfo() const
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

	bool Scene::OnResize(const Event::EventData& data)
	{
		return m_RootNode->OnResize(data);
	}

	bool SceneNode::OnResize(const Event::EventData& data)
	{
		bool resized = false;
		if (m_Camera)
		{
			if (m_Camera->IsAutoAspectRatio())
			{
				u32 width = data.Data.u32[0];
				u32 height = data.Data.u32[1];
				width = width < 1 ? 1 : width;
				height = height < 1 ? 1 : height;
				f32 aspectRatio = static_cast<f32>(width) / static_cast<f32>(height);
				m_Camera->SetAspectRatio(aspectRatio);
			}
			resized = true;
		}
		for (const Scope<SceneNode>& node : m_Children)
		{
			if (node->OnResize(data))
			{
				resized = true;
			}
		}
		return resized;
	}

	const void Scene::PopulateSceneRenderInfo(SceneRenderInfo* sceneRenderInfo, const glm::mat4& transform) const
	{
		sceneRenderInfo->EnvironmentMap = m_EnvironmentMapHandle;

		m_RootNode->PopulateSceneRenderInfo(sceneRenderInfo, transform);
	}

}
