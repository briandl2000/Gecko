#include "imgui.h"
#include "imgui_impl_agc.h"

IMGUI_IMPL_API bool ImGui_ImplAgc_Init()
{

    ImGuiIO& io = ImGui::GetIO();
    IM_ASSERT(io.BackendRendererUserData == nullptr && "Already initialized a renderer backend!");

    io.BackendRendererName = "imgui_impl_Agc";

	io.DisplaySize = {1, 1};
	io.Fonts->Build();

    return true;
}

IMGUI_IMPL_API void ImGui_ImplAgc_Shutdown()
{

}

IMGUI_IMPL_API void ImGui_ImplAgc_NewFrame()
{

}

IMGUI_IMPL_API void ImGui_ImplAgc_RenderDrawData(ImDrawData* draw_data)
{
	
}
