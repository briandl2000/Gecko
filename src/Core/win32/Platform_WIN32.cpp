#ifdef WIN32
#include "Core/Platform.h"
#include "Core/Logger.h"
#include "Core/Event.h"
#include "Core/Input.h"
#include "Core/Asserts.h"

// Windows Includes
#ifndef UNICODE
#define UNICODE
#endif 
#include <Windows.h>
#include <Windowsx.h>

#include <vector>

namespace Gecko { namespace Platform 
{
	// Platform state
	namespace {
		LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

		constexpr wchar_t CLASS_NAME[] = L"Gecko_Window_Class";

		struct PlatformState
		{
			HWND Window{ nullptr };
			HINSTANCE Instance{ nullptr };

			AppInfo Info;
			bool IsClosed = false;
		};

		static Scope<PlatformState> s_State{ nullptr };
	}

	bool Init(AppInfo& info)
	{
		ASSERT(s_State == nullptr, "State already initialized, state must be nullptr when calling Init!");

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
		ASSERT(s_State != nullptr, "Cannot shutdown platform if platform isn't initialized!");
		s_State.release();
	}
	
	const AppInfo& GetAppInfo()
	{
		ASSERT(s_State != nullptr, "Platform state not initialized, did you not run Platform::Init()?");
		return s_State->Info;
	}

	void* GetWindowData()
	{
		ASSERT(s_State != nullptr, "Platform state not initialized, did you not run Platform::Init()?");
		return s_State->Window;
	}

	bool PumpMessage()
	{
		ASSERT(s_State != nullptr, "Platform state not initialized, did you not run Platform::Init()?");
		MSG message;
		while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&message);
			DispatchMessageA(&message);
		}
		return true;
	}

	bool IsRunning()
	{
		ASSERT(s_State != nullptr, "Platform state not initialized, did you not run Platform::Init()?");
		return !s_State->IsClosed;
	}

    namespace
    {
		LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
		{
			Event::EventData data {};
			data.Sender = nullptr;

			switch (uMsg)
			{
			case WM_DESTROY:
			{
				Event::FireEvent(Event::SystemEvent::CODE_WINDOW_CLOSED, data);
				s_State->IsClosed = true;
				return 0;
			} break;
			case WM_SIZE:
			{
				RECT r;
				GetClientRect(hwnd, &r);
				u32 width = r.right - r.left;
				u32 height = r.bottom - r.top;

				data.Data.u32[0] = (u16)width;
				data.Data.u32[1] = (u16)height;
				Event::FireEvent(Event::SystemEvent::CODE_RESIZED, data);

			} break;
			case WM_KEYDOWN:
			case WM_SYSKEYDOWN:
			case WM_KEYUP:
			case WM_SYSKEYUP:
			{
				bool pressed = (uMsg == WM_KEYDOWN || uMsg == WM_SYSKEYDOWN);
				Input::Key key = static_cast<Input::Key>(wParam);

				bool is_extended = (HIWORD(lParam) & KF_EXTENDED) == KF_EXTENDED;

				if (wParam == VK_MENU) {
					key = is_extended ? Input::Key::RALT : Input::Key::LALT;
				}
				else if (wParam == VK_SHIFT) {
					u32 left_shift = MapVirtualKey(VK_LSHIFT, MAPVK_VK_TO_VSC);
					u32 scancode = ((lParam & (0xFF << 16)) >> 16);
					key = scancode == left_shift ? Input::Key::LSHIFT : Input::Key::RSHIFT;
				}
				else if (wParam == VK_CONTROL) {
					key = is_extended ? Input::Key::RCONTROL : Input::Key::LCONTROL;
				}

				data.Data.u32[0] = static_cast<u32>(key);

				Event::FireEvent(pressed ? Event::SystemEvent::CODE_KEY_PRESSED : Event::SystemEvent::CODE_KEY_RELEASED, data);

				return 0;
			} break;
			case WM_MOUSEMOVE:
			{
				// Mouse move
				i32 x = GET_X_LPARAM(lParam);
				i32 y = GET_Y_LPARAM(lParam);
				data.Data.i32[0] = x;
				data.Data.i32[1] = y;

				// Pass over to the input subsystem.
				Event::FireEvent(Event::SystemEvent::CODE_MOUSE_MOVED, data);
			} break;
			case WM_MOUSEWHEEL:
			{
				i32 z_delta = GET_WHEEL_DELTA_WPARAM(wParam);
				if (z_delta != 0) {
					// Flatten the input to an OS-independent (-1, 1)
					z_delta = (z_delta < 0) ? -1 : 1;
					data.Data.i32[0] = z_delta;
					Event::FireEvent(Event::SystemEvent::CODE_MOUSE_MOVED, data);
				}
			} break;
			case WM_LBUTTONDOWN:
			case WM_MBUTTONDOWN:
			case WM_RBUTTONDOWN:
			case WM_LBUTTONUP:
			case WM_MBUTTONUP:
			case WM_RBUTTONUP:
			{
				bool pressed = uMsg == WM_LBUTTONDOWN || uMsg == WM_RBUTTONDOWN || uMsg == WM_MBUTTONDOWN;
				Input::MouseButton mouse_button = Input::MouseButton::MAX_BUTTONS;
				switch (uMsg) {
				case WM_LBUTTONDOWN:
				case WM_LBUTTONUP:
					mouse_button = Input::MouseButton::LEFT;
					break;
				case WM_MBUTTONDOWN:
				case WM_MBUTTONUP:
					mouse_button = Input::MouseButton::MIDDLE;
					break;
				case WM_RBUTTONDOWN:
				case WM_RBUTTONUP:
					mouse_button = Input::MouseButton::RIGHT;
					break;
				}

				// Pass over to the input subsystem.
				if (mouse_button != Input::MouseButton::MAX_BUTTONS)
				{
					Event::FireEvent(pressed ? Event::SystemEvent::CODE_BUTTON_PRESSED : Event::SystemEvent::CODE_BUTTON_RELEASED, data);
				}
			} break;
			}
			return DefWindowProc(hwnd, uMsg, wParam, lParam);
		}
    }

	float GetScreenAspectRatio()
	{
		ASSERT(s_State != nullptr, "Platform state not initialized, did you not run Platform::Init()?");

		return static_cast<float>(s_State->Info.Width) / static_cast<float>(s_State->Info.Height);
	}

	float GetTime()
	{
		ASSERT(s_State != nullptr, "Platform state not initialized, did you not run Platform::Init()?");
		
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
		ASSERT(s_State != nullptr, "Platform state not initialized, did you not run Platform::Init()?");

		return s_State->Info.WorkingDir+filePath;
	}

} 

namespace Logger
{

	void ConsoleWrite(char* msg, Logger::eLogLevel level)
	{
		HANDLE consoleHandle = GetStdHandle(STD_OUTPUT_HANDLE);

		static u8 levels[6] = { 64, 4, 6, 2, 11, 8 };
		SetConsoleTextAttribute(consoleHandle, levels[level]);
		OutputDebugStringA(msg);
		u64 length = strlen(msg);
		LPDWORD numberWritten = 0;
		WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), msg, (DWORD)length, numberWritten, 0);
		/* Reset to debug text (since format from previous level would persist for system messages and
		   into the next session otherwise) */
		SetConsoleTextAttribute(consoleHandle, levels[4]);
		/* Add a break line underneath a fatal message, to break the red marking and
		   separate consecutive fatal messages */
		if (level == LOG_LEVEL_FATAL)
			WriteConsoleA(GetStdHandle(STD_OUTPUT_HANDLE), "\n", (DWORD)1, numberWritten, 0);
	}

} }

#endif // WIN32