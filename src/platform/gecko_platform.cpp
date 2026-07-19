#include "gecko/gecko.h"

#include "input.cpp"
#include "monitors_interface.cpp"
#include "platform_io.cpp"
#include "platform.cpp"
#include "terminal.cpp"
#include "window_event_input.cpp"
#include "windows_interface.cpp"

// WinUser.h maps CreateWindow to CreateWindowA/W. The macro must not leak
// into Gecko's platform interface implementations in this unity unit.
#if defined(CreateWindow)
#undef CreateWindow
#endif

#include "private/null_monitors_backend.cpp"
#include "private/null_windows_interface.cpp"

#include "linux/platform_io_linux.cpp"
#include "linux/shared_library_linux.cpp"
#include "linux/threading_linux.cpp"
#include "linux/wayland_monitors_backend.cpp"
#include "linux/wayland_windows_backend.cpp"

#include "win32/platform_io_win32.cpp"
#include "win32/shared_library_win32.cpp"
#include "win32/threading_win32.cpp"
#include "win32/win32_monitors_backend.cpp"
#include "win32/win32_windows_backend.cpp"

// Xlib defines broad macros such as None, Success, and Bool. Keep all
// Xlib-dependent implementation at the end of the platform unity unit.
#include "platform_config.cpp"
#include "linux/x11_monitors_backend.cpp"
#include "linux/x11_windows_interface.cpp"
#include "clipboard.cpp"
