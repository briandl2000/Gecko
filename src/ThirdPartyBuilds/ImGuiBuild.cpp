#include <imgui.cpp>
#include <imgui_demo.cpp>
#include <imgui_draw.cpp>
#include <imgui_tables.cpp>
#include <imgui_widgets.cpp>

#ifdef WIN32

#pragma warning( push )
#pragma warning(disable : 4189)

#include <backends/imgui_impl_dx12.cpp>
#include <backends/imgui_impl_win32.cpp>

#pragma warning( pop )

#endif // WIN32