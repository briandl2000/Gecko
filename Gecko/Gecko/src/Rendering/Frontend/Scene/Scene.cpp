#include "Rendering/Frontend/Scene/Scene.h"

#include "Platform/Platform.h"
#include <imgui.h>

namespace Gecko 
{

Ref<SceneNode> SceneNode::AddNode()
{
	Ref<SceneNode> newNode = m_Scene->CreateNode(m_Index);

	return newNode;
}

void SceneNode::AddMeshInstance(MeshHandle meshHandle, MaterialHandle materialHandle)
{
	m_MeshesInstances.push_back({meshHandle, materialHandle});
}

void SceneNode::AddLight()
{
}

void SceneNode::AddCamera(f32 FOV, f32 near, f32 far, f32 aspectRatio, bool keepAspect)
{
	hasCamera = true;

	if (keepAspect)
	{
		aspectRatio = Platform::GetScreenAspectRatio();
	}

	camera = { FOV, aspectRatio, near, far, keepAspect };
}

void SceneNode::AddScene(Ref<Scene> scene)
{
	m_Scenes.push_back(scene);
}

void SceneNode::ImGuiRender()
{
	ImGui::Begin("Selected Node");

	if (ImGui::Button("Add Node"))
	{
		AddNode();
	}

	if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_None))
	{
		ImGui::DragFloat3("Position", &Transform.Position[0], 0.1f);
		ImGui::DragFloat3("Rotation", &Transform.Rotation[0], 0.1f);
		ImGui::DragFloat3("Scale", &Transform.Scale[0], 0.1f);
	}

	if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_None))
	{
		if (hasCamera)
		{
			ImGui::DragFloat("FOV", &camera.FieldOfFiew, 0.1f);
			ImGui::DragFloat("Aspect Ratio", &camera.Aspact, 0.1f);
			ImGui::DragFloat("Near", &camera.Near, 0.1f);
			ImGui::DragFloat("Far", &camera.Far, 0.1f);
			ImGui::Checkbox("Screen Aspect", &camera.keepAspect);
		}
	}

	if (ImGui::CollapsingHeader("Meshes", ImGuiTreeNodeFlags_None))
	{
		if (ImGui::Button("Add Mesh"))
		{
			m_MeshesInstances.push_back(MeshInstance());
		}

		for (u32 i = 0; i < m_MeshesInstances.size(); i++)
		{
			ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnArrow |
				ImGuiTreeNodeFlags_OpenOnDoubleClick |
				ImGuiTreeNodeFlags_SpanAvailWidth;
			bool node_open = ImGui::TreeNodeEx((void*)(intptr_t)i, node_flags, "Mesh %d", i);
			if (node_open)
			{

				MeshInstance& meshInstance = m_MeshesInstances[i];

				i32 meshInt = meshInstance.MeshHandle;
				ImGui::InputInt("Mesh Handle", &meshInt);
				meshInstance.MeshHandle = meshInt;

				i32 materialInt = meshInstance.MaterialHandle;
				ImGui::InputInt("Material Handle", &materialInt);
				meshInstance.MaterialHandle = materialInt;

				ImGui::TreePop();
			}
		}
	}
	
	ImGui::End();
}

void Scene::Init()
{
	m_RootNodeIndex = 0;
	CreateNode();
}

Ref<SceneNode> Scene::GetRootNode()
{
	return m_Nodes[m_RootNodeIndex];
}

Ref<SceneNode> Scene::CreateNode(u32 parentIndex)
{
	Ref<SceneNode> node = CreateRef<SceneNode>();
	u32 index = static_cast<u32>(m_Nodes.size());
	m_Nodes.push_back(node);

	node->m_ParentIndex = parentIndex;
	node->m_Index = index;
	node->m_Scene = this;

	return node;
}

const SceneDescriptor Scene::GetSceneDescriptor(glm::mat4 transform) const
{
	SceneDescriptor sceneDescriptor;

	PopulateSceneDescriptor(sceneDescriptor, transform);

	return sceneDescriptor;
}

void Scene::ImGuiRender()
{
	ImGui::Begin("Scene");

	static u32 selectedNode = m_RootNodeIndex;

	if (ImGui::CollapsingHeader("EnvironmentMap", ImGuiTreeNodeFlags_None))
	{

	}

	if (ImGui::CollapsingHeader("Nodes", ImGuiTreeNodeFlags_None))
	{
		if (ImGui::Button("Add Node"))
		{
			CreateNode();
		}
		DisplayNode(m_RootNodeIndex, selectedNode);
	}

	if (selectedNode >= m_Nodes.size())
		selectedNode = m_RootNodeIndex;
	m_Nodes[selectedNode]->ImGuiRender();
	
	ImGui::End();

}

void Scene::DisplayNode(u32 nodeIndex, u32& selectedNode)
{
	static ImGuiTreeNodeFlags base_flags = ImGuiTreeNodeFlags_OpenOnArrow | 
											ImGuiTreeNodeFlags_OpenOnDoubleClick | 
											ImGuiTreeNodeFlags_SpanAvailWidth;

	ImGuiTreeNodeFlags node_flags = base_flags;
	const bool is_selected = nodeIndex == selectedNode;

	if (is_selected)
		node_flags |= ImGuiTreeNodeFlags_Selected;

	// Items 0..2 are Tree Node
	bool node_open = ImGui::TreeNodeEx((void*)(intptr_t)nodeIndex, node_flags, "Node %d", nodeIndex);
	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
		selectedNode = nodeIndex;

	if (node_open)
	{
		for (u32 i = nodeIndex+1; i < m_Nodes.size(); i++)
		{
			Ref<SceneNode>& sceneNode = m_Nodes[i];

			u32 parentIndex = sceneNode->m_ParentIndex;

			if (nodeIndex == parentIndex)
			{
				DisplayNode(i, selectedNode);
			}
		}
		ImGui::TreePop();
	}
}

const void Scene::PopulateSceneDescriptor(SceneDescriptor& sceneDescriptor, glm::mat4 transform) const
{
	sceneDescriptor.EnvironmentMap = EnvironmentMapHandle;

	u32 TransformIndcesOffset = static_cast<u32>(sceneDescriptor.NodeTransformMatrices.size());
	sceneDescriptor.NodeTransformMatrices.resize(TransformIndcesOffset + m_Nodes.size());

	for (u32 i = 0; i < m_Nodes.size(); i++)
	{
		const Ref<SceneNode>& node = m_Nodes[i];
		const NodeTransform& nodeTransform = node->Transform;

		glm::mat4 transformMatrix = nodeTransform.GetMat4();

		glm::mat4 parentMatrix = transform;
		if (i != m_RootNodeIndex)
		{
			parentMatrix = sceneDescriptor.NodeTransformMatrices[TransformIndcesOffset + node->m_ParentIndex];
		}
		glm::mat4 worldMatrix = parentMatrix * transformMatrix;

		u32 worldMatrixIndex = TransformIndcesOffset + i;
		sceneDescriptor.NodeTransformMatrices[worldMatrixIndex] = (worldMatrix);

		// push back all the meshes
		for (const MeshInstance& meshInstance : node->m_MeshesInstances)
		{
			sceneDescriptor.Meshes.push_back({ meshInstance, worldMatrixIndex });
		}

		// TODO: push back the lights

		// check if there is a camera and add it
		if (node->hasCamera && !sceneDescriptor.HasCamera)
		{
			sceneDescriptor.Camera.ViewMatrixIndex = worldMatrixIndex;
			sceneDescriptor.Camera.Projection = glm::perspective(
				glm::radians(node->camera.FieldOfFiew),
				Platform::GetScreenAspectRatio(),
				node->camera.Near,
				node->camera.Far
			);
			sceneDescriptor.HasCamera = true;
		}
	}

	for (u32 i = 0; i < m_Nodes.size(); i++)
	{
		const Ref<SceneNode>& node = m_Nodes[i];

		glm::mat4 worldMatrix = sceneDescriptor.NodeTransformMatrices[TransformIndcesOffset + i];
	
		for (const Ref<Scene>& childScene : node->m_Scenes)
		{
			childScene->PopulateSceneDescriptor(sceneDescriptor, worldMatrix);
		}
	}
}

}