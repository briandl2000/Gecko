#pragma once
#include "imgui.h"

IMGUI_IMPL_API bool     ImGui_ImplAgc_Init();
IMGUI_IMPL_API void     ImGui_ImplAgc_Shutdown();
IMGUI_IMPL_API void     ImGui_ImplAgc_NewFrame();
IMGUI_IMPL_API void     ImGui_ImplAgc_RenderDrawData(ImDrawData* draw_data);