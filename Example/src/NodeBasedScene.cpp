#include "NodeBasedScene.h"

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
using SceneRenderInfo = Gecko::SceneRenderInfo;
#pragma endregion

void NodeBasedScene::Init(const std::string& name)
{
	Gecko::Scene::Init(name);
	m_RootNode = Gecko::CreateScope<SceneNode>();
	m_RootNode->SetName("Root Node");
}

const void NodeBasedScene::PopulateSceneRenderInfo(SceneRenderInfo* sceneRenderInfo, const glm::mat4& transform) const
{
	Gecko::Scene::PopulateSceneRenderInfo(sceneRenderInfo, transform);

	m_RootNode->PopulateSceneRenderInfo(sceneRenderInfo, transform);
}

bool NodeBasedScene::OnResize(const Gecko::Event::EventData& data)
{
	return m_RootNode->OnResize(data);
}

bool SceneNode::OnResize(const Gecko::Event::EventData& data)
{
	bool resized = false;
	if (m_Camera)
	{
		if (m_Camera->IsAutoAspectRatio())
		{
			Gecko::u32 width = data.Data.u32[0];
			Gecko::u32 height = data.Data.u32[1];
			width = width < 1 ? 1 : width;
			height = height < 1 ? 1 : height;
			Gecko::f32 aspectRatio = static_cast<Gecko::f32>(width) / static_cast<Gecko::f32>(height);
			m_Camera->SetAspectRatio(aspectRatio);
		}
		resized = true;
	}
	for (const Gecko::Scope<SceneNode>& node : m_Children)
	{
		if (node->OnResize(data))
		{
			resized = true;
		}
	}
	return resized;
}

SceneNode* NodeBasedScene::GetRootNode() const
{
	return m_RootNode.get();
}

Gecko::Scope<SceneNode> NodeBasedScene::CopySceneToNode() const
{
	Gecko::Scope<SceneNode> newNode = Gecko::CreateScope<SceneNode>();
	newNode->SetName(m_RootNode->GetName());
	m_RootNode->RecursiveCopy(newNode.get());
	return newNode;
}

SceneNode* SceneNode::AddNode(const std::string& name)
{
	Gecko::u32 index = static_cast<Gecko::u32>(m_Children.size());
	m_Children.push_back(Gecko::CreateScope<SceneNode>());

	SceneNode* node = m_Children[index].get();
	node->SetName(name);
	return node;
}

void SceneNode::AppendSceneRenderObject(Gecko::Scope<SceneRenderObject>* sceneRenderObject)
{
	m_SceneRenderObjects.push_back(std::move(*sceneRenderObject));
}

void SceneNode::AppendLight(Gecko::Scope<SceneLight>* light)
{
	m_Lights.push_back(std::move(*light));
}

void SceneNode::AttachCamera(Gecko::Scope<SceneCamera>* camera)
{
	m_Camera = std::move(*camera);
}

void SceneNode::AppendSceneData(const NodeBasedScene& scene)
{
	m_Children.push_back(scene.CopySceneToNode());
}

void SceneNode::RecursiveCopy(SceneNode* target) const
{
	target->Transform = Transform;

	for (const Gecko::Scope<SceneRenderObject>& obj : m_SceneRenderObjects)
	{ // Construct new objects with new Scopes pointing to them, then pass those to the target node
		SceneRenderObject* pObj = new SceneRenderObject(*obj.get());
		Gecko::Scope<SceneRenderObject> sObj = Gecko::CreateScopeFromRaw<SceneRenderObject>(pObj);
		target->m_SceneRenderObjects.push_back(std::move(sObj));
	}

	for (const Gecko::Scope<SceneLight>& light : m_Lights)
	{ // Construct new lights with new Scopes pointing to them, then pass those to the target node
		SceneLight* pLight = new SceneLight(*light.get());
		Gecko::Scope<SceneLight> sLight = Gecko::CreateScopeFromRaw<SceneLight>(pLight);
		target->m_Lights.push_back(std::move(sLight));
	}

	if (m_Camera)
	{ // Construct a new camera with a new Scope pointing to it, then pass that to the target node
		SceneCamera* pCam = new SceneCamera(*m_Camera.get());
		Gecko::Scope<SceneCamera> sCam = Gecko::CreateScopeFromRaw<SceneCamera>(pCam);
		target->AttachCamera(&sCam);
	}

	for (const Gecko::Scope<SceneNode>& c : m_Children)
	{
		SceneNode* n = target->AddNode(c->GetName());
		c->RecursiveCopy(n);
	}
}

Gecko::u32 SceneNode::GetChildrenCount() const
{
	return static_cast<Gecko::u32>(m_Children.size());
}

SceneNode* SceneNode::GetChild(Gecko::u32 childNodeIndex) const
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
	for (const Gecko::Scope<SceneRenderObject>& sceneRenderObject : m_SceneRenderObjects)
	{
		sceneRenderInfo->RenderObjects.push_back({
			sceneRenderObject->GetMeshHandle(),
			sceneRenderObject->GetMaterialHandle(),
			worldMatrix }
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
			LOG_WARN("Unkown light type: %ui!", static_cast<Gecko::u32>(sceneLight->GetLightType()));
			break;
		}
	}

	// Add the children
	for (const Gecko::Scope<SceneNode>& child : m_Children)
	{
		child->PopulateSceneRenderInfo(sceneRenderInfo, worldMatrix);
	}
}