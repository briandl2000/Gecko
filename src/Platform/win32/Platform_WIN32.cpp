#ifdef WIN32
#include "Platform/Platform.h"
#include "Logging/Asserts.h"

// Windows Includes
#ifndef UNICODE
#define UNICODE
#endif 
#include <Windows.h>
#include <Windowsx.h>

#include <vector>

#include <backends/imgui_impl_win32.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace Gecko { namespace Platform 
{
	// Platform state
	namespace {
		LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

		constexpr wchar_t CLASS_NAME[] = L"Gecko_Window_Class";

		struct ResizeEventInfo
		{
			ResizeEventInfo() = default;
			ResizeEvent ResizeEvent{ nullptr };
			void* Data{ nullptr };
		};

		struct PlatformState
		{
			HWND Window{ nullptr };
			HINSTANCE Instance{ nullptr };

			AppInfo Info;
			bool IsClosed = false;

			std::vector<ResizeEventInfo> ResizeEventsInfos;

			bool keys[1000]{ 0 };
		};

		static Scope<PlatformState> s_State{ nullptr };
	}

	bool Init(AppInfo& info)
	{
		ASSERT_MSG(s_State == nullptr, "State already initialized, state must be nullptr when calling Init!");

		s_State = CreateScope<PlatformState>();

		s_State->Info = info;

		// Create Instance and class
		{
			s_State->Instance = GetModuleHandleA(0);

			HICON icon = LoadIcon(s_State->Instance, IDI_APPLICATION);
			WNDCLASSW wc = { };
			wc.style = CS_DBLCLKS;  // Get double-clicks
			wc.lpfnWndProc = WindowProc;
			wc.cbClsExtra = 0;
			wc.cbWndExtra = 0;
			wc.hInstance = s_State->Instance;
			wc.hIcon = icon;
			wc.hCursor = LoadCursor(NULL, IDC_ARROW);  // NULL; // Manage the cursor manually
			wc.hbrBackground = NULL;                   // Transparent
			wc.lpszClassName = CLASS_NAME;

			if (!RegisterClassW(&wc))
			{
				LOG_ERROR("Could not register window class!");
				return false;
			}
		}

		// Create Window
		{
			u32 windowStyle = WS_OVERLAPPED | WS_SYSMENU | WS_CAPTION;
			windowStyle |= WS_MAXIMIZEBOX;
			windowStyle |= WS_MINIMIZEBOX;
			windowStyle |= WS_THICKFRAME;
			u32 windowExStyle = WS_EX_APPWINDOW;

			// Obtain the size of the border.
			RECT borderRect = { 0, 0, 0, 0 };
			AdjustWindowRectEx(&borderRect, windowStyle, 0, windowExStyle);
			i32 x = borderRect.left + s_State->Info.X;
			i32 y = borderRect.top + s_State->Info.Y;
			i32 width = borderRect.right - borderRect.left + s_State->Info.Width;
			i32 height = borderRect.bottom - borderRect.top + s_State->Info.Height;
			
			std::wstring wideStringName = std::wstring(info.Name.begin(), info.Name.end());
			LPCWSTR wideCharName = wideStringName.c_str();

			s_State->Window = CreateWindowExW(
				windowExStyle, 
				CLASS_NAME, 
				wideCharName,
				windowStyle,
				x, 
				y, 
				width, 
				height,
				0, 
				0, 
				s_State->Instance, 
				nullptr
			);

			ShowWindow(s_State->Window, SW_SHOW);
		}


		return true;
	}

	void Shutdown()
	{
		ASSERT_MSG(s_State != nullptr, "Cannot shutdown platform if platform isn't initialized!");
		s_State.release();
	}
	
	const AppInfo& GetAppInfo()
	{
		ASSERT_MSG(s_State != nullptr, "Platform state not initialized, did you not initialize platform?");
		return s_State->Info;
	}

	void* GetWindowData()
	{
		ASSERT_MSG(s_State != nullptr, "Platform state not initialized, did you not initialize platform?");
		return s_State->Window;
	}

	bool PumpMessage()
	{
		ASSERT_MSG(s_State != nullptr, "Platform state not initialized, did you not initialize platform?");
		MSG message;
		while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&message);
			DispatchMessageA(&message);
		}
		return true;
	}

	bool IsRunning()
	{
		ASSERT_MSG(s_State != nullptr, "Platform state not initialized, did you not initialize platform?");
		return !s_State->IsClosed;
	}

	void AddResizeEvent(ResizeEvent resizeEvent, void* listener)
	{
		ASSERT_MSG(s_State != nullptr, "Platform state not initialized, did you not initialize platform?");

		s_State->ResizeEventsInfos.push_back({ resizeEvent, listener });
	}

    namespace
    {
		LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
		{
			if (ImGui_ImplWin32_WndProcHandler(hwnd, uMsg, wParam, lParam))
				return true;

			switch (uMsg)
			{
			case WM_DESTROY:
			{
				s_State->IsClosed = true;
			} break;
			case WM_SIZE:
			{
				RECT r;
				GetClientRect(hwnd, &r);
				u32 width = r.right - r.left;
				u32 height = r.bottom - r.top;

				for (ResizeEventInfo& resizeEventInfo : s_State->ResizeEventsInfos)
				{
					resizeEventInfo.ResizeEvent(width, height, resizeEventInfo.Data);
				}
			} break;
			case WM_KEYDOWN:
			case WM_SYSKEYDOWN:
			case WM_KEYUP:
			case WM_SYSKEYUP:
			{
				bool pressed = (uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN);
				u32 key = (u32)wParam;
				s_State->keys[key] = pressed;

				return 0;
			} break;
			}

			return DefWindowProc(hwnd, uMsg, wParam, lParam);
		}
    }

	float GetScreenAspectRatio()
	{
		ASSERT_MSG(s_State != nullptr, "Platform state not initialized, did you not initialize platform?");

		return static_cast<float>(s_State->Info.Width) / static_cast<float>(s_State->Info.Height);
	}

	float GetTime()
	{
		ASSERT_MSG(s_State != nullptr, "Platform state not initialized, did you not initialize platform?");
		
		static LARGE_INTEGER frequency;
		static LARGE_INTEGER startTime;
		static bool initialized = false;

		if (!initialized) {
			QueryPerformanceFrequency(&frequency);
			QueryPerformanceCounter(&startTime);
			initialized = true;
		}

		LARGE_INTEGER currentTime;
		QueryPerformanceCounter(&currentTime);

		// Calculate the elapsed time in seconds
		double elapsedTime = static_cast<double>(currentTime.QuadPart - startTime.QuadPart) / frequency.QuadPart;

		return static_cast<float>(elapsedTime);
	}

	glm::vec3 GetPositionInput()
	{
		glm::vec3 dir(0.f);
		if (s_State->keys[87])
		{
			dir.z -= 1.f;
		}
		if (s_State->keys[83])
		{
			dir.z += 1.f;
		}
		if (s_State->keys[65])
		{
			dir.x -= 1.f;
		}
		if (s_State->keys[68])
		{
			dir.x += 1.f;
		}
		if (s_State->keys[32])
		{
			dir.y += 1.f;
		}
		if (s_State->keys[16])
		{
			dir.y -= 1.f;
		}

		return dir;
	}
	glm::vec3 GetRotationInput()
	{
		glm::vec3 rot(0.f);
		if (s_State->keys[38])
		{
			rot.x += 1.f;
		}
		if (s_State->keys[40])
		{
			rot.x -= 1.f;
		}
		if (s_State->keys[37])
		{
			rot.y += 1.f;
		}
		if (s_State->keys[39])
		{
			rot.y -= 1.f;
		}
		return rot;
	}

	// Platform specific functions

	void ConsoleWrite(char* msg, Logger::eLogLevel level)
	{
		HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);

		static u8 levels[6] = { 64, 4, 6, 2, 11, 8 };
		SetConsoleTextAttribute(consoleHandle, levels[level]);
		OutputDebugStringA(msg);
		u64 length = strlen(msg);
		LPDWORD numberWritten = 0;
		WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), msg, (DWORD)length, numberWritten, 0);
	}

	void* CustomAllocate(size_t size) {
		return malloc(size);
	}

	void* CustomRealloc(void* mem, size_t size) {
		return realloc(mem, size);
	}

	void CustomFree(void* mem) {
		free(mem);
	}
	
	std::string GetLocalPath(std::string filePath)
	{
		ASSERT(s_State != nullptr);

		return s_State->Info.WorkingDir+filePath;
	}

} }

#endif // WIN32