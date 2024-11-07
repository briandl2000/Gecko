#include "DebugSceneUIRenderer.h"

#include "NodeBasedScene.h"
#include <imgui.h>

void RenderSceneNodeTreeUI(SceneNode* node, SceneNode** selectedNode)
{
	static SceneNode* lastSelectedNode = nullptr;

	ImGui::PushID(node);

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
		ImGuiTreeNodeFlags_OpenOnDoubleClick |
		ImGuiTreeNodeFlags_SpanAvailWidth;

	if (node == lastSelectedNode)
	{
		flags |= ImGuiTreeNodeFlags_Selected;
		*selectedNode = lastSelectedNode;
	}

	if (node->GetChildrenCount() == 0)
	{
		flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		ImGui::TreeNodeEx(node, flags, "%s", node->GetName().c_str());
		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
		{
			lastSelectedNode = node;
			*selectedNode = lastSelectedNode;
		}
	}
	else
	{
		bool node_open = ImGui::TreeNodeEx(node, flags, "%s", node->GetName().c_str());

		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
		{
			lastSelectedNode = node;
			*selectedNode = lastSelectedNode;
		}

		if (node_open)
		{
			for (Gecko::u32 i = 0; i < node->GetChildrenCount(); i++)
			{
				SceneNode* child = node->GetChild(i);
				RenderSceneNodeTreeUI(child, selectedNode);
			}
			ImGui::TreePop();
		}
	}
	ImGui::PopID();
}

void RenderSceneUI(NodeBasedScene* scene, SceneNode** selectedNode)
{
	if (scene == nullptr)
	{
		ImGui::Text("No scene selected.");
		return;
	}

	ImGui::Text("%s", scene->GetName().c_str());

	if (ImGui::CollapsingHeader("Nodes"))
	{
		SceneNode* rootNode = scene->GetRootNode();
		RenderSceneNodeTreeUI(rootNode, selectedNode);
	}

}

void RenderSceneManagerUI(Gecko::SceneManager* sceneManager, SceneNode** selectedNode, NodeBasedScene** selectedScene)
{
	ImGui::Begin("SceneManager");

	if (ImGui::BeginTabBar("SceneTabBar", ImGuiTabBarFlags_None))
	{
		for (Gecko::u32 i = 0; i < sceneManager->GetSceneCount(); i++)
		{
			ImGui::PushID(i);
			NodeBasedScene* scene = sceneManager->GetScene<NodeBasedScene>(i);
			if (ImGui::BeginTabItem(scene->GetName().c_str()))
			{
				ImGui::Separator();
				RenderSceneUI(scene, selectedNode);
				*selectedScene = scene;
				ImGui::EndTabItem();
			}
			ImGui::PopID();
		}
		ImGui::EndTabBar();
	}

	ImGui::End();

}

void RenderTransformUI(NodeTransform& transform)
{
	ImGui::DragFloat3("Position", &transform.Position[0], 0.01f);

	ImGui::DragFloat3("Rotation", &transform.Rotation[0], 1.f);

	ImGui::DragFloat3("Scale", &transform.Scale[0], 0.01f);
}

void RenderSelectedNodeUI(SceneNode* node, NodeBasedScene* scene)
{
	ImGui::Begin("Node");
	if (node == nullptr)
	{
		ImGui::Text("No node selected");
		ImGui::End();
		return;
	}

	ImGui::Text("%s", node->GetName().c_str());

	if (ImGui::CollapsingHeader("Transform"))
	{
		RenderTransformUI(node->Transform);
	}

	if (ImGui::CollapsingHeader("Meshes"))
	{
		if (ImGui::Button("Add Mesh"))
		{

		}
	}

	if (ImGui::CollapsingHeader("Lights"))
	{
		if (ImGui::Button("Add Light"))
		{

		}
	}

	if (ImGui::CollapsingHeader("Cameras"))
	{
	}


	ImGui::End();
}

bool DebugSceneUIRenderer::RenderDebugUI(Gecko::ApplicationContext& ctx)
{
	if (Gecko::DebugUIRenderer::RenderDebugUI(ctx))
	{
		SceneNode* selectedNode = nullptr;
		NodeBasedScene* selectedScene = nullptr;
		RenderSceneManagerUI(ctx.GetSceneManager(), &selectedNode, &selectedScene);
		RenderSelectedNodeUI(selectedNode, selectedScene);
		return true;
	}
	else
	{
		return false;
	}
}
