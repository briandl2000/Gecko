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

	bool DebugUIRenderer::RenderDebugUI(ApplicationContext& ctx)
	{
		if (SetupDockSpace())
		{
			ImGui::ShowDemoWindow();

			RenderRendererUI(ctx.GetRenderer());
			RenderResourceManagerUI(ctx.GetResourceManager());
			return true;
		}
		else
		{
			return false;
		}
	}

}