# Windowing & Monitor System

Gecko's platform module provides a cross-platform windowing and monitor API. The system abstracts over native backends (Win32, Wayland, X11, Cocoa) behind a unified interface, with event-driven input and display management.

## Quick start

```cpp
#include <gecko/platform/platform_context.h>
#include <gecko/platform/platform_module.h>

using namespace gecko;
using namespace gecko::platform;

// After GECKO_BOOT and module installation...

// 1. Create a platform context (auto-detects best backend)
PlatformConfig cfg = {};
cfg.Backend = DisplayBackendKind::Auto;
PlatformContext ctx(cfg);

// 2. Describe and create a window
WindowDesc desc;
desc.Title   = "My App";
desc.Size    = {1280, 720};
desc.Visible = true;

WindowHandle window = ctx.Windows().CreateWindow(desc);
if (!window.IsValid())
{
  // handle error
}

// 3. Subscribe to the close button
bool running = true;
auto closeSub = gecko::SubscribeEvent(
    events::WindowCloseRequested,
    [](void* user, const gecko::EventMeta&, gecko::EventView) {
      *static_cast<bool*>(user) = false;
    },
    &running);

// 4. Run the event loop
while (running && ctx.Windows().IsWindowAlive(window))
{
  ctx.PumpEvents();
  gecko::DispatchEvents();
  // ... render, update, etc.
}

// 5. Tear down
ctx.Windows().DestroyWindow(window);
```

See `examples/platform_example/src/main.cpp` for a complete, runnable version with logging, profiling and service setup.

---

## Backends

Gecko selects a display backend at runtime based on the platform and available display servers.

| Backend | Platform | Notes |
|---------|----------|-------|
| Win32   | Windows  | Primary Windows backend |
| Wayland | Linux    | Preferred when `WAYLAND_DISPLAY` is set |
| X11     | Linux    | Fallback when Wayland is unavailable |
| Cocoa   | macOS    | Not yet implemented |
| Null    | Any      | Headless stub — all operations succeed silently |

Set `PlatformConfig::Backend` to `DisplayBackendKind::Auto` (the default) and the platform module will pick the right one. You can force a specific backend for testing:

```cpp
cfg.Backend = DisplayBackendKind::Null; // headless, no real windows
```

---

## Window creation

### WindowDesc

All window properties are optional — sensible defaults are provided.

| Field       | Type          | Default              | Description |
|-------------|---------------|----------------------|-------------|
| `Title`     | `const char*` | `"Gecko"`            | Title bar text |
| `Size`      | `math::Int2`  | `{1280, 720}`        | Initial client area size in pixels |
| `Mode`      | `WindowMode`  | `Windowed`           | `Windowed`, `Fullscreen`, or `BorderlessFullscreen` |
| `Resizable` | `bool`        | `true`               | Allow user resizing |
| `Visible`   | `bool`        | `true`               | Show immediately after creation |
| `Decorated` | `bool`        | `true`               | Show title bar and borders |
| `HighDpi`   | `bool`        | `true`               | Enable high-DPI scaling |

```cpp
WindowDesc desc;
desc.Title     = "Editor";
desc.Size      = {1920, 1080};
desc.Mode      = WindowMode::Windowed;
desc.Resizable = true;
desc.Visible   = true;
desc.Decorated = true;

WindowHandle window = ctx.Windows().CreateWindow(desc);
```

### WindowHandle

Windows are identified by opaque `WindowHandle` values. Check validity with `IsValid()`:

```cpp
WindowHandle window; // default-constructed = invalid
assert(!window.IsValid());

window = ctx.Windows().CreateWindow(desc);
assert(window.IsValid());  // now valid
```

---

## Window properties

After creation, query and modify window properties through the `IWindowsBackend` interface (accessed via `ctx.Windows()`).

### Size and position

```cpp
// Get/set the client area size (excludes title bar and borders)
Extent2D size = ctx.Windows().GetClientSize(window);
ctx.Windows().SetClientSize(window, {800, 600});

// Get/set the window position in desktop coordinates
math::Int2 pos = ctx.Windows().GetPosition(window);
ctx.Windows().SetPosition(window, {100, 100});
```

> **Note:** on Wayland, `SetPosition` and `GetPosition` are no-ops — the compositor controls window placement.

### Title

```cpp
ctx.Windows().SetTitle(window, "New Title");
const char* title = ctx.Windows().GetTitle(window);
```

### DPI

```cpp
DpiInfo dpi = ctx.Windows().GetDpi(window);
// dpi.Dpi   = 96, 120, 144, 192, ...
// dpi.Scale = 1.0, 1.25, 1.5, 2.0, ...
```

DPI can change at runtime (e.g. dragging a window between monitors). Subscribe to `WindowDpiChanged` to react.

### Decorations

Toggle the title bar and window borders:

```cpp
ctx.Windows().SetDecorated(window, false); // borderless
bool decorated = ctx.Windows().IsDecorated(window);
```

### Window state

Minimize, maximize, restore, or hide:

```cpp
ctx.Windows().SetWindowState(window, WindowState::Minimized);
ctx.Windows().SetWindowState(window, WindowState::Maximized);
ctx.Windows().SetWindowState(window, WindowState::Normal);    // restore
ctx.Windows().SetWindowState(window, WindowState::Hidden);

WindowState state = ctx.Windows().GetWindowState(window);
```

### Focus

```cpp
ctx.Windows().RequestFocus(window);
```

### Cursor mode

```cpp
ctx.Windows().SetCursorMode(window, CursorMode::Normal);  // visible, free
ctx.Windows().SetCursorMode(window, CursorMode::Hidden);   // invisible
ctx.Windows().SetCursorMode(window, CursorMode::Locked);   // locked to center

CursorMode mode = ctx.Windows().GetCursorMode(window);
```

### Native handle

For interop with graphics APIs (Vulkan, DirectX, etc.):

```cpp
NativeWindowHandle native = ctx.Windows().GetNativeWindowHandle(window);
// native.Backend  — DisplayBackendKind (Win32, Wayland, Xcb, ...)
// native.Handle   — platform-specific (HWND, wl_surface*, xcb_window_t, ...)
// native.Display  — platform-specific (nullptr, wl_display*, xcb_connection_t*, ...)
```

---

## Window lifecycle

```cpp
// Check if the window still exists
bool alive = ctx.Windows().IsWindowAlive(window);

// Request a close (fires WindowCloseRequested event)
ctx.Windows().RequestClose(window);

// Destroy the window immediately
ctx.Windows().DestroyWindow(window);
```

The typical pattern is to let the user close via the title bar button, handle `WindowCloseRequested` in your event loop, then call `DestroyWindow` during teardown.

---

## Multiple windows

You can create and manage multiple windows simultaneously:

```cpp
WindowHandle mainWindow, toolWindow, previewWindow;
ctx.Windows().CreateWindow(mainDesc,    mainWindow);
ctx.Windows().CreateWindow(toolDesc,    toolWindow);
ctx.Windows().CreateWindow(previewDesc, previewWindow);

// Each window has its own handle — all events include a WindowHandle
// so you can tell which window they came from
```

---

## Events

All window and monitor events flow through the global event bus. Use `gecko::SubscribeEvent()` to register callbacks. Events are dispatched in two phases:

1. **`ctx.PumpEvents()`** — polls the OS for new input and queues events
2. **`gecko::DispatchEvents()`** — delivers queued events to subscribers

Every event payload carries a `WindowHandle` and a `TimeNs` timestamp.

### Window events

| Event | Payload | Description |
|-------|---------|-------------|
| `WindowCloseRequested` | `WindowCloseRequestedPayload` | User clicked the close button |
| `WindowClosed` | `WindowClosedPayload` | Window was destroyed |
| `WindowResized` | `WindowResizedPayload` | Client area size changed |
| `WindowMoved` | `WindowMovedPayload` | Window position changed |
| `WindowStateChanged` | `WindowStateChangedPayload` | Minimized, maximized, restored |
| `WindowFocusChanged` | `WindowFocusChangedPayload` | Window gained or lost focus |
| `WindowDpiChanged` | `WindowDpiChangedPayload` | DPI/scale factor changed |
| `WindowKey` | `WindowKeyPayload` | Key pressed or released |
| `WindowChar` | `WindowCharPayload` | Text input (UTF-32 codepoint) |
| `WindowMouseMove` | `WindowMouseMovePayload` | Mouse cursor moved |
| `WindowMouseButton` | `WindowMouseButtonPayload` | Mouse button pressed or released |
| `WindowMouseWheel` | `WindowMouseWheelPayload` | Scroll wheel |

### Monitor events

| Event | Payload | Description |
|-------|---------|-------------|
| `MonitorConnected` | `MonitorConnectedPayload` | A new monitor was plugged in |
| `MonitorDisconnected` | `MonitorDisconnectedPayload` | A monitor was removed |
| `MonitorReconfigured` | `MonitorReconfiguredPayload` | Resolution, DPI, or layout changed |

### Subscribing to events

```cpp
#include <gecko/platform/platform_events.h>

// Simple close handler
auto closeSub = gecko::SubscribeEvent(
    events::WindowCloseRequested,
    [](void* user, const gecko::EventMeta&, gecko::EventView) {
      *static_cast<bool*>(user) = false;
    },
    &running);

// Keyboard input
auto keySub = gecko::SubscribeEvent(
    events::WindowKey,
    [](void* /*user*/, const gecko::EventMeta&, gecko::EventView view) {
      const auto* p =
          reinterpret_cast<const events::WindowKeyPayload*>(view.Data());
      if (p->Down && p->Key == KeyCode::Escape)
      {
        // handle escape
      }
    },
    nullptr);

// Window resize
auto resizeSub = gecko::SubscribeEvent(
    events::WindowResized,
    [](void* /*user*/, const gecko::EventMeta&, gecko::EventView view) {
      const auto* p =
          reinterpret_cast<const events::WindowResizedPayload*>(view.Data());
      // rebuild swapchain for new size: p->Width x p->Height
    },
    nullptr);
```

> **Lifetime:** keep `EventSubscription` objects alive for as long as you want to receive events. They automatically unsubscribe on destruction.

### Immediate vs queued delivery

By default, events are queued and delivered during `DispatchEvents()`. For latency-sensitive handling, request immediate delivery:

```cpp
auto sub = gecko::SubscribeEvent(
    events::WindowKey, myCallback, nullptr,
    {.delivery = SubscriptionOptions::SubscriptionDelivery::Immediate});
```

Immediate callbacks fire during `PumpEvents()` on the calling thread.

---

## Event payloads reference

### Input payloads

```cpp
struct WindowKeyPayload {
  WindowHandle Window;
  u64 TimeNs;
  KeyCode Key;     // see KeyCode enum
  u8 Down;         // 1 = pressed, 0 = released
  u8 Repeat;       // 1 = OS key repeat, 0 = first press
};

struct WindowCharPayload {
  WindowHandle Window;
  u64 TimeNs;
  u32 Codepoint;   // UTF-32 character
};

struct WindowMouseMovePayload {
  WindowHandle Window;
  u64 TimeNs;
  i32 X;           // client-area coordinates
  i32 Y;
};

struct WindowMouseButtonPayload {
  WindowHandle Window;
  u64 TimeNs;
  MouseButton Button;  // Left, Right, Middle, X1, X2
  u8 Down;             // 1 = pressed, 0 = released
};

struct WindowMouseWheelPayload {
  WindowHandle Window;
  u64 TimeNs;
  float DeltaX;    // horizontal scroll
  float DeltaY;    // vertical scroll (positive = up)
};
```

### Window state payloads

```cpp
struct WindowResizedPayload {
  WindowHandle Window;
  u64 TimeNs;
  u32 Width;       // new client area width
  u32 Height;      // new client area height
};

struct WindowMovedPayload {
  WindowHandle Window;
  u64 TimeNs;
  i32 X;           // new position
  i32 Y;
};

struct WindowStateChangedPayload {
  WindowHandle Window;
  u64 TimeNs;
  WindowState OldState;
  WindowState NewState;
};

struct WindowFocusChangedPayload {
  WindowHandle Window;
  u64 TimeNs;
  u8 Focused;      // 1 = gained focus, 0 = lost focus
};

struct WindowDpiChangedPayload {
  WindowHandle Window;
  u64 TimeNs;
  u32 Dpi;
  float Scale;
};
```

---

## Monitor system

Query connected monitors through `ctx.Monitors()`.

### Enumerating monitors

```cpp
ctx.Monitors().EnumerateMonitors();

u32 count = ctx.Monitors().GetMonitorCount();
for (u32 i = 0; i < count; ++i)
{
  MonitorHandle handle = ctx.Monitors().GetMonitorHandle(i);

  MonitorInfo info = ctx.Monitors().GetMonitorProperties(handle);

  // info.Name            — display name
  // info.Bounds          — full display area (Rect2D)
  // info.WorkArea        — usable area (excludes taskbar)
  // info.RefreshRateMilliHz — e.g. 60000 = 60 Hz, 144000 = 144 Hz
  // info.Dpi             — e.g. 96, 192
  // info.DpiScale        — e.g. 1.0, 2.0
  // info.ColorSpace      — Srgb, Hdr10, DolbyVision
  // info.IsPrimary       — true for the primary display
}
```

### Primary monitor

```cpp
MonitorHandle primary = ctx.Monitors().GetPrimaryMonitor();

MonitorInfo info = ctx.Monitors().GetMonitorProperties(primary);
```

### Monitor bounds

```cpp
MonitorHandle handle = ctx.Monitors().GetMonitorHandle(0);
MonitorBounds mb = ctx.Monitors().GetMonitorBounds(handle);

// mb.Bounds.Position — top-left in desktop coordinates
// mb.Bounds.Size     — full resolution
// mb.WorkArea        — excludes system UI (taskbar, dock, etc.)
```

### Monitor hotplug events

Subscribe to `MonitorConnected`, `MonitorDisconnected`, and `MonitorReconfigured` to react to display changes at runtime:

```cpp
auto monitorSub = gecko::SubscribeEvent(
    events::MonitorConnected,
    [](void*, const gecko::EventMeta&, gecko::EventView view) {
      const auto* p =
          reinterpret_cast<const events::MonitorConnectedPayload*>(view.Data());
      // p->Monitor — handle to the new monitor
      // p->Info    — full MonitorInfo
    },
    nullptr);
```

---

## Input codes

### KeyCode

Key codes use values that map directly to Win32 virtual key codes. On other platforms (Wayland, X11), native scancodes are translated to these values.

Common key codes:

| Key | Value | Key | Value |
|-----|-------|-----|-------|
| `A`–`Z` | `0x41`–`0x5A` | `F1`–`F12` | `0x70`–`0x7B` |
| `D0`–`D9` | `0x30`–`0x39` | `Numpad0`–`Numpad9` | `0x60`–`0x69` |
| `Escape` | `0x1B` | `Space` | `0x20` |
| `Enter` | `0x0D` | `Tab` | `0x09` |
| `Backspace` | `0x08` | `Delete` | `0x2E` |
| `Shift` | `0x10` | `Control` | `0x11` |
| `Alt` | `0x12` | `CapsLock` | `0x14` |
| `Left` | `0x25` | `Up` | `0x26` |
| `Right` | `0x27` | `Down` | `0x28` |

Left/right modifiers are also available: `LeftShift`, `RightShift`, `LeftControl`, `RightControl`, `LeftAlt`, `RightAlt`.

### MouseButton

```cpp
enum class MouseButton : u8 {
  Left   = 0,
  Right  = 1,
  Middle = 2,
  X1     = 3,
  X2     = 4,
};
```

---

## Platform configuration

### PlatformConfig

```cpp
struct PlatformConfig {
  DisplayBackendKind Backend {DisplayBackendKind::Auto};
  WindowConfig Window;
  MonitorConfig Monitor;
};

struct WindowConfig {
  bool EnableHighDpi {true};
};

struct MonitorConfig {
  bool EnableHotplugEvents {true};
};
```

### Backend selection

`DisplayBackendKind::Auto` selects the best available backend:

| Platform | Selection logic |
|----------|----------------|
| Windows  | Win32 |
| Linux    | Wayland if `WAYLAND_DISPLAY` is set, otherwise X11 |
| macOS    | Cocoa (not yet implemented) |

If the selected backend is unavailable at runtime, falls back to `Null`.

---

## Module installation

The platform system is packaged as a module. Install it after `GECKO_BOOT`:

```cpp
#include <gecko/platform/platform_module.h>

// After GECKO_BOOT(...)
(void)InstallModule(platform::GetModule());
```

The module handles internal initialization and cleanup. `PlatformContext` can be created after the module is installed.

---

## Platform-specific notes

### Wayland

- Window positioning (`SetPosition`/`GetPosition`) is ignored — Wayland compositors control placement.
- Decoration toggling uses `xdg_toplevel_set_decorated` if the compositor supports server-side decorations.
- Keyboard input uses `xkb` for keymap processing and dead-key composition.

### Win32

- Key codes are identity-mapped to VK codes (no translation needed).
- DPI awareness is per-monitor via `GetDpiForMonitor` (Shcore).
- `WM_DPICHANGED` automatically repositions the window to the compositor's suggested rect.

### X11

- Uses XCB for the connection and event handling.
- RandR is used for monitor enumeration and hotplug detection.

### Null backend

The null backend is a no-op stub. `CreateWindow` succeeds and returns a valid handle, but no real window is shown and no OS events are generated. Useful for unit tests and headless CI.
