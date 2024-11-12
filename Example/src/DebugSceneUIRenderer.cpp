#include "DebugSceneUIRenderer.h"

#include "NodeBasedScene.h"
#include "FlatHierarchyScene.h"
#include <imgui.h>

void RenderTransformUI(Gecko::Transform& transform)
{
	ImGui::DragFloat3("Position", &transform.Position[0], 0.01f);

	ImGui::DragFloat3("Rotation", &transform.Rotation[0], 1.f);

	ImGui::DragFloat3("Scale", &transform.Scale[0], 0.01f);
}

void RenderTransformWindowUI(Gecko::Transform& transform)
{
	ImGui::Begin("Transform");
	RenderTransformUI(transform);
	ImGui::End();
}

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

void RenderSceneContainerUI(FlatHierarchyScene* scene, void* selectedItem)
{
	static void* lastSelectedItem = nullptr;

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
		ImGuiTreeNodeFlags_OpenOnDoubleClick |
		ImGuiTreeNodeFlags_SpanAvailWidth;

	bool objectsOpen = ImGui::TreeNodeEx(scene + 0, flags, "Objects");
	if (objectsOpen)
	{
		const std::vector<Gecko::Scope<Gecko::SceneRenderObject>>& objects = scene->GetSceneObjects();
		for (Gecko::u32 i = 0; i < objects.size(); i++)
		{
			const auto& obj = objects[i];
			ImGuiTreeNodeFlags objFlags;
			objFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
			if (obj.get() == lastSelectedItem)
			{
				objFlags |= ImGuiTreeNodeFlags_Selected;
				RenderTransformWindowUI(obj->GetModifiableTransform());
			}

			ImGui::TreeNodeEx(&obj, flags | objFlags, "SceneObject %u", i);
			if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
			{
				lastSelectedItem = reinterpret_cast<void*>(obj.get());
			}
		}
		ImGui::TreePop();
	}

	bool lightsOpen = ImGui::TreeNodeEx(scene + 1, flags, "Lights");
	if (lightsOpen)
	{
		const std::vector<Gecko::Scope<Gecko::SceneLight>>& lights = scene->GetSceneLights();
		for (Gecko::u32 i = 0; i < lights.size(); i++)
		{
			const auto& light = lights[i];
			ImGuiTreeNodeFlags lightFlags;
			lightFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
			if (light.get() == lastSelectedItem)
			{
				lightFlags |= ImGuiTreeNodeFlags_Selected;
				RenderTransformWindowUI(light->GetModifiableTransform());
			}

			const char* lightName = "SceneLight %u";
			if (dynamic_cast<Gecko::SceneDirectionalLight*>(light.get()))
				lightName = "SceneDirectionalLight %u";
			else if (dynamic_cast<Gecko::ScenePointLight*>(light.get()))
				lightName = "ScenePointLight %u";
			else if (dynamic_cast<Gecko::SceneSpotLight*>(light.get()))
				lightName = "SceneSpotLight %u";

			ImGui::TreeNodeEx(&light, flags | lightFlags, lightName, i);
			if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
			{
				lastSelectedItem = reinterpret_cast<void*>(light.get());
			}
		}
		ImGui::TreePop();
	}

	bool additionalCamsOpen = ImGui::TreeNodeEx(scene + 2, flags, "Additional Cameras");
	if (additionalCamsOpen)
	{
		const std::vector<Gecko::Scope<Gecko::SceneCamera>>& cams = scene->GetSceneCameras();
		for (Gecko::u32 i = 0; i < cams.size(); i++)
		{
			const auto& cam = cams[i];
			ImGuiTreeNodeFlags camFlags;
			camFlags = ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
			if (cam.get() == lastSelectedItem)
			{
				camFlags |= ImGuiTreeNodeFlags_Selected;
				RenderTransformWindowUI(cam->GetModifiableTransform());
			}

			ImGui::TreeNodeEx(&cam, flags | camFlags, "SceneCamera %u", i);
			if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
			{
				lastSelectedItem = reinterpret_cast<void*>(cam.get());
			}
		}
		ImGui::TreePop();
	}

	if (const auto& mainCam = scene->GetMainCamera())
	{
		flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		if (mainCam.get() == lastSelectedItem)
		{
			flags |= ImGuiTreeNodeFlags_Selected;
			RenderTransformWindowUI(mainCam->GetModifiableTransform());
		}
		ImGui::TreeNodeEx(scene + 3, flags, "Main Camera");
		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
		{
			lastSelectedItem = reinterpret_cast<void*>(mainCam.get());
		}
	}
}

void RenderSceneUI(Gecko::Scene* scene, SceneNode** selectedNode)
{
	ImGui::Text("%s", scene->GetName().c_str());

	if (ImGui::CollapsingHeader("Scene"))
	{
		if (NodeBasedScene* nodeScene = dynamic_cast<NodeBasedScene*>(scene))
		{
			RenderSceneNodeTreeUI(nodeScene->GetRootNode(), selectedNode);
		}
		else if (FlatHierarchyScene* flatScene = dynamic_cast<FlatHierarchyScene*>(scene))
		{
			RenderSceneContainerUI(flatScene, nullptr);
		}
	}
}

void RenderSceneManagerUI(Gecko::SceneManager* sceneManager, SceneNode** selectedNode, Gecko::Scene** selectedScene)
{
	ImGui::Begin("SceneManager");

	if (ImGui::BeginTabBar("SceneTabBar", ImGuiTabBarFlags_None))
	{
		for (Gecko::u32 i = 0; i < sceneManager->GetSceneCount(); i++)
		{
			ImGui::PushID(i);
			Gecko::Scene* scene = sceneManager->GetScene<Gecko::Scene>(i);
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

void RenderSelectedNodeUI(SceneNode* node)
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
		RenderTransformUI(node->GetModifiableTransform());
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
		Gecko::Scene* selectedScene = nullptr;
		RenderSceneManagerUI(ctx.GetSceneManager(), &selectedNode, &selectedScene);
		RenderSelectedNodeUI(selectedNode);
		return true;
	}
	else
	{
		return false;
	}
}
