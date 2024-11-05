#include "UI/DebugUIRenderer.h"

#include "Rendering/Frontend/ApplicationContext.h"

#include <imgui.h>

namespace Gecko {

	bool SetupDockSpace()
	{
		static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None | ImGuiDockNodeFlags_PassthruCentralNode;
		static bool showImGui = false;

		ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBackground
			| ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize
			| ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

		const ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->WorkPos);
		ImGui::SetNextWindowSize(viewport->WorkSize);
		ImGui::SetNextWindowViewport(viewport->ID);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.f);

		ImGui::Begin("DockSpace Demo", nullptr, window_flags);

		ImGui::PopStyleVar(4);

		ImGuiIO& io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
		{
			ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
			ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
		}

		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 1.f);
		if (ImGui::BeginMenuBar())
		{
			ImGui::Checkbox("Show Imgui", &showImGui);

			ImGui::EndMenuBar();
		}
		ImGui::PopStyleVar(1);
		ImGui::GetStyle().Alpha = .7f;

		ImGui::End();

		return showImGui;
	}

	void RenderResourceManagerUI(ResourceManager* resourceManager)
	{
		ImGui::Begin("Resource Manager");

		ImGui::End();
	}

	void RenderRendererUI(Renderer* renderer)
	{
		ImGui::Begin("Renderer");

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
				for (u32 i = 0; i < node->GetChildrenCount(); i++)
				{
					SceneNode* child = node->GetChild(i);
					RenderSceneNodeTreeUI(child, selectedNode);
				}
				ImGui::TreePop();
			}
		}
		ImGui::PopID();
	}

	void RenderSceneUI(Scene* scene, SceneNode** selectedNode)
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

	void RenderSceneManagerUI(SceneManager* sceneManager, SceneNode** selectedNode, Scene** selectedScene)
	{
		ImGui::Begin("SceneManager");

		if (ImGui::BeginTabBar("SceneTabBar", ImGuiTabBarFlags_None))
		{
			for (u32 i = 0; i < sceneManager->GetSceneCount(); i++)
			{
				ImGui::PushID(i);
				Scene* scene = sceneManager->GetScene(i);
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

	void RenderSelectedNodeUI(SceneNode* node, Scene* scene)
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

	void DebugUIRenderer::RenderDebugUI(ApplicationContext& ctx)
	{
		if (SetupDockSpace())
		{
			ImGui::ShowDemoWindow();

			SceneNode* selectedNode = nullptr;
			Scene* selectedScene = nullptr;
			RenderSceneManagerUI(ctx.GetSceneManager(), &selectedNode, &selectedScene);
			RenderSelectedNodeUI(selectedNode, selectedScene);
			RenderRendererUI(ctx.GetRenderer());
			RenderResourceManagerUI(ctx.GetResourceManager());
		}
	}

}