#include "wayland_windows_backend.h"

#if defined(GECKO_PLATFORM_LINUX) && defined(GECKO_PLATFORM_LINUX_WAYLAND)

#include "../private/labels.h"
#include "../private/platform_utils.h"
#include "gecko/core/ptr.h"
#include "gecko/core/scope.h"
#include "gecko/core/services/log.h"
#include "gecko/platform/input_codes.h"

#include <cstdint>
#include <poll.h>
#include <sys/mman.h>
#include <unistd.h>

namespace gecko::platform {

namespace {

constexpr i32 DefaultCursorSize = 24;
constexpr u32 PlaceholderBufferColor = 0xFF333333;  // ARGB dark grey

// -- Xkbcommon key mapping ----------------------------------------------

#if defined(GECKO_HAS_XKBCOMMON)
KeyCode XkbKeysymToKeyCode(xkb_keysym_t sym) noexcept
{
  // Letters
  if (sym >= XKB_KEY_a && sym <= XKB_KEY_z)
    return static_cast<KeyCode>(0x41 + (sym - XKB_KEY_a));
  if (sym >= XKB_KEY_A && sym <= XKB_KEY_Z)
    return static_cast<KeyCode>(0x41 + (sym - XKB_KEY_A));

  // Digits
  if (sym >= XKB_KEY_0 && sym <= XKB_KEY_9)
    return static_cast<KeyCode>(0x30 + (sym - XKB_KEY_0));

  // Numpad digits
  if (sym >= XKB_KEY_KP_0 && sym <= XKB_KEY_KP_9)
    return static_cast<KeyCode>(0x60 + (sym - XKB_KEY_KP_0));

  // Function keys
  if (sym >= XKB_KEY_F1 && sym <= XKB_KEY_F12)
    return static_cast<KeyCode>(0x70 + (sym - XKB_KEY_F1));

  switch (sym)
  {
  case XKB_KEY_BackSpace:
    return KeyCode::Backspace;
  case XKB_KEY_Tab:
    return KeyCode::Tab;
  case XKB_KEY_Return:
  case XKB_KEY_KP_Enter:
    return KeyCode::Enter;
  case XKB_KEY_Escape:
    return KeyCode::Escape;
  case XKB_KEY_space:
    return KeyCode::Space;
  case XKB_KEY_Page_Up:
    return KeyCode::PageUp;
  case XKB_KEY_Page_Down:
    return KeyCode::PageDown;
  case XKB_KEY_End:
    return KeyCode::End;
  case XKB_KEY_Home:
    return KeyCode::Home;
  case XKB_KEY_Left:
    return KeyCode::Left;
  case XKB_KEY_Up:
    return KeyCode::Up;
  case XKB_KEY_Right:
    return KeyCode::Right;
  case XKB_KEY_Down:
    return KeyCode::Down;
  case XKB_KEY_Insert:
    return KeyCode::Insert;
  case XKB_KEY_Delete:
    return KeyCode::Delete;
  case XKB_KEY_Shift_L:
    return KeyCode::LeftShift;
  case XKB_KEY_Shift_R:
    return KeyCode::RightShift;
  case XKB_KEY_Control_L:
    return KeyCode::LeftControl;
  case XKB_KEY_Control_R:
    return KeyCode::RightControl;
  case XKB_KEY_Alt_L:
    return KeyCode::LeftAlt;
  case XKB_KEY_Alt_R:
    return KeyCode::RightAlt;
  case XKB_KEY_Super_L:
    return KeyCode::LeftSuper;
  case XKB_KEY_Super_R:
    return KeyCode::RightSuper;
  case XKB_KEY_Caps_Lock:
    return KeyCode::CapsLock;
  case XKB_KEY_Num_Lock:
    return KeyCode::NumLock;
  case XKB_KEY_Scroll_Lock:
    return KeyCode::ScrollLock;
  case XKB_KEY_Print:
    return KeyCode::PrintScreen;
  case XKB_KEY_Pause:
    return KeyCode::Pause;
  case XKB_KEY_Menu:
    return KeyCode::Menu;
  case XKB_KEY_minus:
    return KeyCode::Minus;
  case XKB_KEY_equal:
    return KeyCode::Equal;
  case XKB_KEY_bracketleft:
    return KeyCode::LeftBracket;
  case XKB_KEY_bracketright:
    return KeyCode::RightBracket;
  case XKB_KEY_backslash:
    return KeyCode::Backslash;
  case XKB_KEY_semicolon:
    return KeyCode::Semicolon;
  case XKB_KEY_apostrophe:
    return KeyCode::Apostrophe;
  case XKB_KEY_grave:
    return KeyCode::GraveAccent;
  case XKB_KEY_comma:
    return KeyCode::Comma;
  case XKB_KEY_period:
    return KeyCode::Period;
  case XKB_KEY_slash:
    return KeyCode::Slash;
  default:
    return KeyCode::Unknown;
  }
}
#endif

MouseButton WaylandButtonToMouseButton(u32 button) noexcept
{
  // linux/input-event-codes.h: BTN_LEFT=0x110, BTN_RIGHT=0x111,
  // BTN_MIDDLE=0x112
  switch (button)
  {
  case 0x110:
    return MouseButton::Left;
  case 0x111:
    return MouseButton::Right;
  case 0x112:
    return MouseButton::Middle;
  case 0x113:
    return MouseButton::X1;
  case 0x114:
    return MouseButton::X2;
  default:
    return MouseButton::Left;
  }
}

}  // namespace

// -- Listener callbacks (static) ----------------------------------------

namespace {

// Registry
void RegistryGlobal(void* data, ::wl_registry* reg, u32 name, const char* iface, u32 version)
{
  static_cast<WaylandWindowsBackend*>(data)->OnRegistryGlobal(reg, name, iface, version);
}
void RegistryGlobalRemove(void* data, ::wl_registry* reg, u32 name)
{
  static_cast<WaylandWindowsBackend*>(data)->OnRegistryGlobalRemove(reg, name);
}

const wl_registry_listener kRegistryListener = {RegistryGlobal, RegistryGlobalRemove};

// WmBase ping
void WmBasePing(void* /*data*/, ::xdg_wm_base* wmBase, u32 serial)
{
  ::xdg_wm_base_pong(wmBase, serial);
}

const xdg_wm_base_listener kWmBaseListener = {WmBasePing};

// XDG surface configure
// Toplevel configure + close
void ToplevelConfigure(void* data, ::xdg_toplevel* /*toplevel*/, i32 width, i32 height, wl_array* states)
{
  auto* ld = static_cast<ToplevelListenerData*>(data);
  ld->Backend->OnToplevelConfigure(ld->State, width, height, states);
}
void ToplevelClose(void* data, ::xdg_toplevel* /*toplevel*/)
{
  auto* ld = static_cast<ToplevelListenerData*>(data);
  ld->Backend->OnToplevelClose(ld->State);
}
void ToplevelConfigureBounds(void* /*data*/, ::xdg_toplevel* /*toplevel*/, i32 /*width*/, i32 /*height*/)
{}
void ToplevelWmCapabilities(void* /*data*/, ::xdg_toplevel* /*toplevel*/, wl_array* /*caps*/)
{}

const xdg_toplevel_listener kToplevelListener = {ToplevelConfigure, ToplevelClose, ToplevelConfigureBounds,
                                                 ToplevelWmCapabilities};

// Seat capabilities
void SeatCapabilities(void* data, ::wl_seat* seat, u32 caps)
{
  static_cast<WaylandWindowsBackend*>(data)->OnSeatCapabilities(seat, caps);
}
void SeatName(void* /*data*/, ::wl_seat* /*seat*/, const char* /*name*/)
{}

const wl_seat_listener kSeatListener = {SeatCapabilities, SeatName};

// Keyboard
void KbKeymap(void* data, ::wl_keyboard* kb, u32 fmt, i32 fd, u32 sz)
{
  static_cast<WaylandWindowsBackend*>(data)->OnKeyboardKeymap(kb, fmt, fd, sz);
}
void KbEnter(void* data, ::wl_keyboard* kb, u32 serial, ::wl_surface* s, wl_array* keys)
{
  static_cast<WaylandWindowsBackend*>(data)->OnKeyboardEnter(kb, serial, s, keys);
}
void KbLeave(void* data, ::wl_keyboard* kb, u32 serial, ::wl_surface* s)
{
  static_cast<WaylandWindowsBackend*>(data)->OnKeyboardLeave(kb, serial, s);
}
void KbKey(void* data, ::wl_keyboard* kb, u32 serial, u32 time, u32 key, u32 state)
{
  static_cast<WaylandWindowsBackend*>(data)->OnKeyboardKey(kb, serial, time, key, state);
}
void KbModifiers(void* data, ::wl_keyboard* kb, u32 serial, u32 dep, u32 lat, u32 lock, u32 group)
{
  static_cast<WaylandWindowsBackend*>(data)->OnKeyboardModifiers(kb, serial, dep, lat, lock, group);
}
void KbRepeatInfo(void* /*data*/, ::wl_keyboard* /*kb*/, i32 /*rate*/, i32 /*delay*/)
{}

const wl_keyboard_listener kKeyboardListener = {KbKeymap, KbEnter, KbLeave, KbKey, KbModifiers, KbRepeatInfo};

// Pointer
void PtrEnter(void* data, ::wl_pointer* p, u32 serial, ::wl_surface* s, wl_fixed_t sx, wl_fixed_t sy)
{
  static_cast<WaylandWindowsBackend*>(data)->OnPointerEnter(p, serial, s, sx, sy);
}
void PtrLeave(void* data, ::wl_pointer* p, u32 serial, ::wl_surface* s)
{
  static_cast<WaylandWindowsBackend*>(data)->OnPointerLeave(p, serial, s);
}
void PtrMotion(void* data, ::wl_pointer* p, u32 time, wl_fixed_t sx, wl_fixed_t sy)
{
  static_cast<WaylandWindowsBackend*>(data)->OnPointerMotion(p, time, sx, sy);
}
void PtrButton(void* data, ::wl_pointer* p, u32 serial, u32 time, u32 button, u32 state)
{
  static_cast<WaylandWindowsBackend*>(data)->OnPointerButton(p, serial, time, button, state);
}
void PtrAxis(void* data, ::wl_pointer* p, u32 time, u32 axis, wl_fixed_t value)
{
  static_cast<WaylandWindowsBackend*>(data)->OnPointerAxis(p, time, axis, value);
}
void PtrFrame(void* /*data*/, ::wl_pointer* /*p*/)
{}
void PtrAxisSource(void* /*data*/, ::wl_pointer* /*p*/, u32 /*source*/)
{}
void PtrAxisStop(void* /*data*/, ::wl_pointer* /*p*/, u32 /*time*/, u32 /*axis*/)
{}
void PtrAxisDiscrete(void* /*data*/, ::wl_pointer* /*p*/, u32 /*axis*/, i32 /*discrete*/)
{}
void PtrAxisValue120(void* /*data*/, ::wl_pointer* /*p*/, u32 /*axis*/, i32 /*value120*/)
{}
void PtrAxisRelativeDirection(void* /*data*/, ::wl_pointer* /*p*/, u32 /*axis*/, u32 /*direction*/)
{}

const wl_pointer_listener kPointerListener = {PtrEnter,
                                              PtrLeave,
                                              PtrMotion,
                                              PtrButton,
                                              PtrAxis,
                                              PtrFrame,
                                              PtrAxisSource,
                                              PtrAxisStop,
                                              PtrAxisDiscrete,
                                              PtrAxisValue120,
                                              PtrAxisRelativeDirection};

}  // namespace

// -- Constructor / Destructor -------------------------------------------

WaylandWindowsBackend::WaylandWindowsBackend() noexcept
{
  m_Display = ::wl_display_connect(nullptr);
  if (!m_Display)
  {
    GECKO_ERROR(labels::General, "Failed to connect to Wayland display");
    return;
  }

  m_Registry = ::wl_display_get_registry(m_Display);
  ::wl_registry_add_listener(m_Registry, &kRegistryListener, this);
  ::wl_display_roundtrip(m_Display);

  if (!m_WmBase)
  {
    GECKO_ERROR(labels::General, "Compositor does not support xdg_wm_base");
    return;
  }

  if (!m_Compositor)
  {
    GECKO_ERROR(labels::General, "Compositor does not provide wl_compositor");
    return;
  }

  // Load cursor theme for default cursor.
  if (m_Shm)
  {
    m_CursorTheme = ::wl_cursor_theme_load(nullptr, DefaultCursorSize, m_Shm);
    if (m_CursorTheme)
      m_CursorSurface = ::wl_compositor_create_surface(m_Compositor);
  }

#if defined(GECKO_HAS_XKBCOMMON)
  m_XkbContext = ::xkb_context_new(XKB_CONTEXT_NO_FLAGS);
#endif

  GECKO_INFO(labels::General, "Initialized Wayland windows backend (display=%p)", m_Display);
}

WaylandWindowsBackend::~WaylandWindowsBackend() noexcept
{
  // Destroy windows first.
  for (auto& [id, ws] : m_Windows)
  {
    if (ws.Decoration)
      ::zxdg_toplevel_decoration_v1_destroy(ws.Decoration);
    if (ws.Toplevel)
      ::xdg_toplevel_destroy(ws.Toplevel);
    if (ws.XdgSurface)
      ::xdg_surface_destroy(ws.XdgSurface);
    if (ws.Surface)
      ::wl_surface_destroy(ws.Surface);
  }
  m_Windows.clear();
  m_WindowBySurface.clear();

#if defined(GECKO_HAS_XKBCOMMON)
  if (m_XkbState)
    ::xkb_state_unref(m_XkbState);
  if (m_XkbKeymap)
    ::xkb_keymap_unref(m_XkbKeymap);
  if (m_XkbContext)
    ::xkb_context_unref(m_XkbContext);
#endif

  if (m_Pointer)
    ::wl_pointer_destroy(m_Pointer);
  if (m_Keyboard)
    ::wl_keyboard_destroy(m_Keyboard);
  if (m_CursorSurface)
    ::wl_surface_destroy(m_CursorSurface);
  if (m_CursorTheme)
    ::wl_cursor_theme_destroy(m_CursorTheme);
  if (m_DecorationManager)
    ::zxdg_decoration_manager_v1_destroy(m_DecorationManager);
  if (m_Seat)
    ::wl_seat_destroy(m_Seat);
  if (m_Shm)
    wl_shm_destroy(m_Shm);
  if (m_WmBase)
    ::xdg_wm_base_destroy(m_WmBase);
  if (m_Compositor)
    wl_compositor_destroy(m_Compositor);
  if (m_Registry)
    ::wl_registry_destroy(m_Registry);
  if (m_Display)
  {
    ::wl_display_disconnect(m_Display);
    m_Display = nullptr;
  }
}

// -- Registry callbacks -------------------------------------------------

void WaylandWindowsBackend::OnRegistryGlobal(::wl_registry* registry, u32 name, const char* interface,
                                             u32 version) noexcept
{
  if (::std::strcmp(interface, wl_compositor_interface.name) == 0)
  {
    m_Compositor = static_cast<::wl_compositor*>(::wl_registry_bind(registry, name, &wl_compositor_interface, 4));
  }
  else if (::std::strcmp(interface, wl_shm_interface.name) == 0)
  {
    m_Shm = static_cast<::wl_shm*>(::wl_registry_bind(registry, name, &wl_shm_interface, 1));
  }
  else if (::std::strcmp(interface, xdg_wm_base_interface.name) == 0)
  {
    m_WmBase = static_cast<::xdg_wm_base*>(::wl_registry_bind(registry, name, &xdg_wm_base_interface, 1));
    ::xdg_wm_base_add_listener(m_WmBase, &kWmBaseListener, this);
  }
  else if (::std::strcmp(interface, wl_seat_interface.name) == 0)
  {
    m_Seat = static_cast<::wl_seat*>(::wl_registry_bind(registry, name, &wl_seat_interface, 5));
    ::wl_seat_add_listener(m_Seat, &kSeatListener, this);
  }
#ifdef GECKO_HAVE_XDG_DECORATION
  else if (::std::strcmp(interface, zxdg_decoration_manager_v1_interface.name) == 0)
  {
    m_DecorationManager = static_cast<::zxdg_decoration_manager_v1*>(
        ::wl_registry_bind(registry, name, &zxdg_decoration_manager_v1_interface, 1));
  }
#endif
}

void WaylandWindowsBackend::OnRegistryGlobalRemove(::wl_registry* /*registry*/, u32 /*name*/) noexcept
{}

// -- Seat capabilities --------------------------------------------------

void WaylandWindowsBackend::OnSeatCapabilities(::wl_seat* seat, u32 caps) noexcept
{
  const bool hasKeyboard = (caps & WL_SEAT_CAPABILITY_KEYBOARD) != 0;
  const bool hasPointer = (caps & WL_SEAT_CAPABILITY_POINTER) != 0;

  if (hasKeyboard && !m_Keyboard)
  {
    m_Keyboard = ::wl_seat_get_keyboard(seat);
    ::wl_keyboard_add_listener(m_Keyboard, &kKeyboardListener, this);
  }
  else if (!hasKeyboard && m_Keyboard)
  {
    ::wl_keyboard_destroy(m_Keyboard);
    m_Keyboard = nullptr;
  }

  if (hasPointer && !m_Pointer)
  {
    m_Pointer = ::wl_seat_get_pointer(seat);
    ::wl_pointer_add_listener(m_Pointer, &kPointerListener, this);
  }
  else if (!hasPointer && m_Pointer)
  {
    ::wl_pointer_destroy(m_Pointer);
    m_Pointer = nullptr;
  }
}

// -- XDG surface / toplevel callbacks -----------------------------------

// Static ::xdg_surface listener -- data is WaylandWindowState*
namespace {
void XdgSurfaceConfigureCb(void* data, ::xdg_surface* /*surface*/, u32 serial)
{
  auto* ws = static_cast<WaylandWindowState*>(data);
  ws->PendingSerial = serial;
  ws->ConfigurePending = true;
}

const xdg_surface_listener kXdgSurfaceListener = {XdgSurfaceConfigureCb};
}  // namespace

void WaylandWindowsBackend::OnToplevelConfigure(WaylandWindowState* ws, i32 width, i32 height,
                                                wl_array* /*states*/) noexcept
{
  if (width > 0 && height > 0)
  {
    ws->PendingWidth = width;
    ws->PendingHeight = height;
  }
}

void WaylandWindowsBackend::OnToplevelClose(WaylandWindowState* ws) noexcept
{
  m_Staged.push_back(MakeWaylandStagedEvent(
      events::WindowCloseRequested, events::WindowCloseRequestedPayload {WindowHandle {ws->GeckoId}, NowNsSafe()}));
}

// -- Window management --------------------------------------------------

WindowHandle WaylandWindowsBackend::CreateWindow(const WindowDesc& desc) noexcept
{
  GECKO_SCOPE(labels::General);

  if (!m_Display || !m_Compositor || !m_WmBase)
    return {};

  const u64 id = ++m_NextId;

  auto& ws = m_Windows[id];
  ws.GeckoId = id;
  ws.Desc = desc;
  ws.TitleStorage = desc.Title ? desc.Title : "Gecko";
  ws.Desc.Title = ws.TitleStorage.c_str();
  ws.ClientSize = {static_cast<u32>(desc.Size.X > 0 ? desc.Size.X : 1280),
                   static_cast<u32>(desc.Size.Y > 0 ? desc.Size.Y : 720)};
  ws.Decorated = desc.Decorated;
  ws.Resizable = desc.Resizable;
  ws.Mode = desc.Mode;
  ws.Buttons = desc.Buttons;
  ws.State = desc.Visible ? platform::WindowState::Normal : platform::WindowState::Hidden;
  ws.Alive = true;

  // Create wl_surface.
  ws.Surface = ::wl_compositor_create_surface(m_Compositor);
  if (!ws.Surface)
  {
    GECKO_ERROR(labels::General, "Failed to create wl_surface");
    m_Windows.erase(id);
    return {};
  }
  m_WindowBySurface.emplace(ws.Surface, id);

  // Create xdg_surface.
  ws.XdgSurface = ::xdg_wm_base_get_xdg_surface(m_WmBase, ws.Surface);
  if (!ws.XdgSurface)
  {
    GECKO_ERROR(labels::General, "Failed to create xdg_surface");
    m_WindowBySurface.erase(ws.Surface);
    ::wl_surface_destroy(ws.Surface);
    m_Windows.erase(id);
    return {};
  }
  ::xdg_surface_add_listener(ws.XdgSurface, &kXdgSurfaceListener, &ws);

  // Create xdg_toplevel.
  ws.Toplevel = ::xdg_surface_get_toplevel(ws.XdgSurface);
  if (!ws.Toplevel)
  {
    GECKO_ERROR(labels::General, "Failed to create xdg_toplevel");
    m_WindowBySurface.erase(ws.Surface);
    ::xdg_surface_destroy(ws.XdgSurface);
    ::wl_surface_destroy(ws.Surface);
    m_Windows.erase(id);
    return {};
  }

  // Store listener data for toplevel callbacks.
  // We use a static map to keep listener data alive.
  static ::std::unordered_map<u64, ToplevelListenerData> s_ToplevelData;
  s_ToplevelData[id] = {this, &ws};
  ::xdg_toplevel_add_listener(ws.Toplevel, &kToplevelListener, &s_ToplevelData[id]);

  // Set title.
  ::xdg_toplevel_set_title(ws.Toplevel, desc.Title ? desc.Title : "Gecko");

  // Apply resize constraints.
  if (!desc.Resizable)
  {
    ::xdg_toplevel_set_min_size(ws.Toplevel, static_cast<i32>(ws.ClientSize.Width),
                                static_cast<i32>(ws.ClientSize.Height));
    ::xdg_toplevel_set_max_size(ws.Toplevel, static_cast<i32>(ws.ClientSize.Width),
                                static_cast<i32>(ws.ClientSize.Height));
  }

  // Apply window mode (fullscreen).
  if (desc.Mode == WindowMode::Fullscreen || desc.Mode == WindowMode::BorderlessFullscreen)
  {
    ::xdg_toplevel_set_fullscreen(ws.Toplevel, nullptr);
  }

  // Request decoration mode.
  // BorderlessFullscreen implies no decorations.
  const bool wantDecorations = desc.Decorated && desc.Mode != WindowMode::BorderlessFullscreen;
  ws.Decorated = wantDecorations;
#ifdef GECKO_HAVE_XDG_DECORATION
  if (m_DecorationManager)
  {
    ws.Decoration = ::zxdg_decoration_manager_v1_get_toplevel_decoration(m_DecorationManager, ws.Toplevel);
    ::zxdg_toplevel_decoration_v1_set_mode(ws.Decoration, wantDecorations
                                                              ? ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE
                                                              : ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
  }
#endif

  // Commit the surface to trigger the initial configure.
  ::wl_surface_commit(ws.Surface);
  ::wl_display_roundtrip(m_Display);

  // Handle the initial configure.
  if (ws.ConfigurePending)
  {
    if (ws.PendingWidth > 0 && ws.PendingHeight > 0)
    {
      ws.ClientSize = {static_cast<u32>(ws.PendingWidth), static_cast<u32>(ws.PendingHeight)};
    }
    ::xdg_surface_ack_configure(ws.XdgSurface, ws.PendingSerial);
    ws.ConfigurePending = false;
    ws.Configured = true;
  }

  // Attach a blank buffer so the compositor will actually map the window.
  if (desc.Visible)
    AttachBlankBuffer(ws);

  GECKO_INFO(labels::Window, "Created Wayland window id=%llu, surface=%p, size=%ux%u",
             static_cast<unsigned long long>(id), static_cast<void*>(ws.Surface), ws.ClientSize.Width,
             ws.ClientSize.Height);
  return WindowHandle {id};
}

void WaylandWindowsBackend::DestroyWindow(WindowHandle window) noexcept
{
  GECKO_SCOPE(labels::General);

  if (!window.IsValid())
    return;

  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return;

  auto& ws = it->second;
  ws.Alive = false;

  if (ws.Decoration)
    ::zxdg_toplevel_decoration_v1_destroy(ws.Decoration);
  if (ws.Toplevel)
    ::xdg_toplevel_destroy(ws.Toplevel);
  if (ws.XdgSurface)
    ::xdg_surface_destroy(ws.XdgSurface);

  if (ws.Surface)
  {
    m_WindowBySurface.erase(ws.Surface);
    ::wl_surface_destroy(ws.Surface);
  }

  if (m_Display)
    ::wl_display_flush(m_Display);

  m_Staged.push_back(MakeWaylandStagedEvent(events::WindowClosed, events::WindowClosedPayload {window, NowNsSafe()}));

  m_Windows.erase(it);
}

bool WaylandWindowsBackend::IsWindowAlive(WindowHandle window) const noexcept
{
  if (!window.IsValid())
    return false;
  return m_Windows.find(window.Id) != m_Windows.end();
}

bool WaylandWindowsBackend::RequestClose(WindowHandle window) noexcept
{
  GECKO_SCOPE(labels::General);

  if (!IsWindowAlive(window))
    return false;

  m_Staged.push_back(
      MakeWaylandStagedEvent(events::WindowCloseRequested, events::WindowCloseRequestedPayload {window, NowNsSafe()}));
  return true;
}

// -- Window properties --------------------------------------------------

Extent2D WaylandWindowsBackend::GetClientSize(WindowHandle window) const noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return Extent2D {};
  return it->second.ClientSize;
}

void WaylandWindowsBackend::SetClientSize(WindowHandle window, Extent2D size) noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return;

  // On Wayland, we can't directly resize. We set the size and commit.
  // The compositor may or may not honour it.
  it->second.ClientSize = size;
  if (it->second.Surface)
    ::wl_surface_commit(it->second.Surface);
}

void WaylandWindowsBackend::SetTitle(WindowHandle window, const char* title) noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return;

  it->second.TitleStorage = title ? title : "";
  it->second.Desc.Title = it->second.TitleStorage.c_str();
  if (it->second.Toplevel && title)
    ::xdg_toplevel_set_title(it->second.Toplevel, title);
}

const char* WaylandWindowsBackend::GetTitle(WindowHandle window) const noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return "";
  return it->second.TitleStorage.c_str();
}

void WaylandWindowsBackend::SetPosition(WindowHandle window, math::Int2 pos) noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return;

  // Wayland clients cannot position their own windows.
  // We store the value for GetPosition() but cannot affect the compositor.
  it->second.Position = pos;
}

math::Int2 WaylandWindowsBackend::GetPosition(WindowHandle window) const noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return math::Int2 {0, 0};
  return it->second.Position;
}

DpiInfo WaylandWindowsBackend::GetDpi(WindowHandle /*window*/) const noexcept
{
  // TODO: query ::wl_output scale for the surface's current output.
  // Requires tracking which ::wl_output each surface is on via
  // wl_surface.enter/leave events.
  GECKO_DEBUG(labels::Window, "GetDpi: not yet implemented on Wayland, "
                              "returning default 96 DPI / 1.0x scale");
  return DpiInfo {};
}

NativeWindowHandle WaylandWindowsBackend::GetNativeWindowHandle(WindowHandle window) const noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return NativeWindowHandle {};

  NativeWindowHandle nh;
  nh.Backend = DisplayBackendKind::Wayland;
  nh.Display = m_Display;
  nh.Handle = it->second.Surface;
  return nh;
}

// -- Window state -------------------------------------------------------

void WaylandWindowsBackend::SetWindowState(WindowHandle window, platform::WindowState state) noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return;

  auto& ws = it->second;
  const auto oldState = ws.State;
  if (oldState == state)
    return;

  if (!ws.Toplevel)
    return;

  switch (state)
  {
  case platform::WindowState::Normal:
    if (oldState == platform::WindowState::Maximized)
      ::xdg_toplevel_unset_maximized(ws.Toplevel);
    if (oldState == platform::WindowState::Minimized)
    {
      // There's no "un-minimize" in xdg-shell; best we can do is nothing.
      // The user must click the taskbar to restore.
    }
    break;

  case platform::WindowState::Minimized:
    ::xdg_toplevel_set_minimized(ws.Toplevel);
    break;

  case platform::WindowState::Maximized:
    ::xdg_toplevel_set_maximized(ws.Toplevel);
    break;

  case platform::WindowState::Hidden:
    // Hiding on Wayland requires unmapping the xdg-surface (destroy +
    // recreate), which is a heavyweight operation with side-effects.
    // For now, store state only and log a warning.
    GECKO_WARN(labels::Window, "WindowState::Hidden is not fully implemented on Wayland; "
                               "the window will remain visible");
    break;
  }

  if (m_Display)
    ::wl_display_flush(m_Display);

  ws.State = state;
  m_Staged.push_back(MakeWaylandStagedEvent(events::WindowStateChanged,
                                            events::WindowStateChangedPayload {window, NowNsSafe(), oldState, state}));
}

platform::WindowState WaylandWindowsBackend::GetWindowState(WindowHandle window) const noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return platform::WindowState::Normal;
  return it->second.State;
}

void WaylandWindowsBackend::SetDecorated(WindowHandle window, bool decorated) noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return;

  it->second.Decorated = decorated;

#ifdef GECKO_HAVE_XDG_DECORATION
  if (it->second.Decoration)
  {
    ::zxdg_toplevel_decoration_v1_set_mode(it->second.Decoration, decorated
                                                                      ? ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE
                                                                      : ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE);
  }
#endif
}

bool WaylandWindowsBackend::IsDecorated(WindowHandle window) const noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return true;
  return it->second.Decorated;
}

void WaylandWindowsBackend::RequestFocus(WindowHandle /*window*/) noexcept
{
  // Wayland does not allow clients to steal focus.
  // This is a no-op by design.
}

void WaylandWindowsBackend::SetResizable(WindowHandle window, bool resizable) noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return;

  it->second.Resizable = resizable;

  if (it->second.Toplevel)
  {
    if (resizable)
    {
      Extent2D mn = it->second.MinSize;
      Extent2D mx = it->second.MaxSize;
      ::xdg_toplevel_set_min_size(it->second.Toplevel, mn.Width > 0 ? static_cast<i32>(mn.Width) : 1,
                                  mn.Height > 0 ? static_cast<i32>(mn.Height) : 1);
      ::xdg_toplevel_set_max_size(it->second.Toplevel, mx.Width > 0 ? static_cast<i32>(mx.Width) : 0,
                                  mx.Height > 0 ? static_cast<i32>(mx.Height) : 0);
    }
    else
    {
      // Lock to current size.
      const i32 w = static_cast<i32>(it->second.ClientSize.Width);
      const i32 h = static_cast<i32>(it->second.ClientSize.Height);
      ::xdg_toplevel_set_min_size(it->second.Toplevel, w, h);
      ::xdg_toplevel_set_max_size(it->second.Toplevel, w, h);
    }
    if (m_Display)
      ::wl_display_flush(m_Display);
  }
}

bool WaylandWindowsBackend::IsResizable(WindowHandle window) const noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return true;
  return it->second.Resizable;
}

void WaylandWindowsBackend::SetWindowMode(WindowHandle window, WindowMode mode) noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end() || !it->second.Toplevel)
    return;

  const WindowMode old = it->second.Mode;
  if (old == mode)
    return;

  // Unset current mode.
  if (old == WindowMode::Fullscreen || old == WindowMode::BorderlessFullscreen)
    ::xdg_toplevel_unset_fullscreen(it->second.Toplevel);

  switch (mode)
  {
  case WindowMode::Windowed:
    break;
  case WindowMode::Fullscreen:
  case WindowMode::BorderlessFullscreen:
    ::xdg_toplevel_set_fullscreen(it->second.Toplevel, nullptr);
    break;
  }

  it->second.Mode = mode;
  if (m_Display)
    ::wl_display_flush(m_Display);
}

WindowMode WaylandWindowsBackend::GetWindowMode(WindowHandle window) const noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return WindowMode::Windowed;
  return it->second.Mode;
}

void WaylandWindowsBackend::SetWindowButtons(WindowHandle window, WindowButtons buttons) noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return;

  it->second.Buttons = buttons;
  // Wayland does not expose per-button CSD control directly.
  // This is stored for API consistency and can be used by CSD renderers.
}

WindowButtons WaylandWindowsBackend::GetWindowButtons(WindowHandle window) const noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return WindowButtons::All;
  return it->second.Buttons;
}

void WaylandWindowsBackend::SetMinSize(WindowHandle window, Extent2D size) noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return;

  it->second.MinSize = size;
  if (it->second.Toplevel && it->second.Resizable)
  {
    ::xdg_toplevel_set_min_size(it->second.Toplevel, size.Width > 0 ? static_cast<i32>(size.Width) : 1,
                                size.Height > 0 ? static_cast<i32>(size.Height) : 1);
    if (m_Display)
      ::wl_display_flush(m_Display);
  }
}

void WaylandWindowsBackend::SetMaxSize(WindowHandle window, Extent2D size) noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return;

  it->second.MaxSize = size;
  if (it->second.Toplevel && it->second.Resizable)
  {
    ::xdg_toplevel_set_max_size(it->second.Toplevel, size.Width > 0 ? static_cast<i32>(size.Width) : 0,
                                size.Height > 0 ? static_cast<i32>(size.Height) : 0);
    if (m_Display)
      ::wl_display_flush(m_Display);
  }
}

void WaylandWindowsBackend::SetAlwaysOnTop(WindowHandle /*window*/, bool /*topmost*/) noexcept
{
  // Wayland does not support always-on-top from the client side.
  // This is a no-op by design.
}

bool WaylandWindowsBackend::IsAlwaysOnTop(WindowHandle window) const noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return false;
  return it->second.AlwaysOnTop;
}

void WaylandWindowsBackend::SetCursorMode(WindowHandle window, CursorMode mode) noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return;

  it->second.Cursor = mode;
  ApplyCursorVisibility(it->second);
}

CursorMode WaylandWindowsBackend::GetCursorMode(WindowHandle window) const noexcept
{
  auto it = m_Windows.find(window.Id);
  if (it == m_Windows.end())
    return CursorMode::Normal;
  return it->second.Cursor;
}

void WaylandWindowsBackend::ApplyCursorVisibility(WaylandWindowState& ws) noexcept
{
  if (!m_Pointer)
    return;

  if (ws.Cursor == CursorMode::Hidden || ws.Cursor == CursorMode::Locked)
  {
    // Hide cursor by setting a nil surface.
    wl_pointer_set_cursor(m_Pointer, m_LastPointerSerial, nullptr, 0, 0);
  }
  else
  {
    // Restore default cursor.
    if (m_CursorTheme && m_CursorSurface)
    {
      ::wl_cursor* cursor = ::wl_cursor_theme_get_cursor(m_CursorTheme, "left_ptr");
      if (cursor && cursor->image_count > 0)
      {
        ::wl_cursor_image* image = cursor->images[0];
        ::wl_buffer* buffer = ::wl_cursor_image_get_buffer(image);
        ::wl_surface_attach(m_CursorSurface, buffer, 0, 0);
        ::wl_surface_damage(m_CursorSurface, 0, 0, static_cast<i32>(image->width), static_cast<i32>(image->height));
        ::wl_surface_commit(m_CursorSurface);
        wl_pointer_set_cursor(m_Pointer, m_LastPointerSerial, m_CursorSurface, static_cast<i32>(image->hotspot_x),
                              static_cast<i32>(image->hotspot_y));
      }
    }
  }
}

void WaylandWindowsBackend::AttachBlankBuffer(WaylandWindowState& ws) noexcept
{
  if (!m_Shm || !ws.Surface)
    return;

  const i32 w = static_cast<i32>(ws.ClientSize.Width);
  const i32 h = static_cast<i32>(ws.ClientSize.Height);
  const i32 stride = w * 4;
  const i32 size = stride * h;

  // Create a temporary anonymous file for the shared-memory buffer.
  int fd = -1;
#if defined(GECKO_PLATFORM_LINUX)
  fd = memfd_create("gecko-wl-buffer", MFD_CLOEXEC);
#endif
  if (fd < 0)
    return;
  if (ftruncate(fd, size) < 0)
  {
    close(fd);
    return;
  }

  void* data = mmap(nullptr, static_cast<size_t>(size), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (data == MAP_FAILED)
  {
    close(fd);
    return;
  }

  // Fill placeholder buffer.
  auto* pixels = static_cast<u32*>(data);
  const i32 count = w * h;
  for (i32 i = 0; i < count; ++i)
    pixels[i] = PlaceholderBufferColor;

  ::wl_shm_pool* pool = wl_shm_create_pool(m_Shm, fd, size);
  ::wl_buffer* buffer = ::wl_shm_pool_create_buffer(pool, 0, w, h, stride, WL_SHM_FORMAT_ARGB8888);
  ::wl_shm_pool_destroy(pool);
  munmap(data, static_cast<size_t>(size));
  close(fd);

  ::wl_surface_attach(ws.Surface, buffer, 0, 0);
  ::wl_surface_damage(ws.Surface, 0, 0, w, h);
  ::wl_surface_commit(ws.Surface);

  // The buffer can be destroyed after commit -- the compositor keeps a ref.
  ::wl_buffer_destroy(buffer);
}

// -- Event pump ---------------------------------------------------------

void WaylandWindowsBackend::PumpEvents(const gecko::EventEmitter& emitter) noexcept
{
  GECKO_SCOPE(labels::General);

  // Flush deferred events first.
  for (const auto& ev : m_Staged)
    gecko::SendEvent(emitter, ev.Code, gecko::EventView {ev.PayloadStorage, ev.PayloadSize});
  m_Staged.clear();

  if (!m_Display)
    return;

  // Non-blocking dispatch: prepare + read (if ready) + dispatch.
  {
    GECKO_PROFILE_NAMED(labels::General, "wl_display_flush+read");
    ::wl_display_flush(m_Display);
    if (::wl_display_prepare_read(m_Display) == 0)
    {
      // Poll with zero timeout (non-blocking).
      struct pollfd pfd {};
      pfd.fd = ::wl_display_get_fd(m_Display);
      pfd.events = POLLIN;
      if (::poll(&pfd, 1, 0) > 0)
        ::wl_display_read_events(m_Display);
      else
        ::wl_display_cancel_read(m_Display);
    }
  }
  {
    GECKO_PROFILE_NAMED(labels::General, "wl_display_dispatch_pending");
    ::wl_display_dispatch_pending(m_Display);
  }

  // Process configure events.
  for (auto& [id, ws] : m_Windows)
  {
    if (!ws.ConfigurePending)
      continue;

    const u32 oldW = ws.ClientSize.Width;
    const u32 oldH = ws.ClientSize.Height;
    if (ws.PendingWidth > 0 && ws.PendingHeight > 0)
    {
      ws.ClientSize = {static_cast<u32>(ws.PendingWidth), static_cast<u32>(ws.PendingHeight)};
    }

    ::xdg_surface_ack_configure(ws.XdgSurface, ws.PendingSerial);
    ws.ConfigurePending = false;
    ws.Configured = true;

    if (ws.ClientSize.Width != oldW || ws.ClientSize.Height != oldH)
    {
      gecko::SendEvent(
          emitter, events::WindowResized,
          events::WindowResizedPayload {WindowHandle {id}, NowNsSafe(), ws.ClientSize.Width, ws.ClientSize.Height});
    }
  }
}

// -- Keyboard callbacks -------------------------------------------------

void WaylandWindowsBackend::OnKeyboardKeymap(::wl_keyboard* /*kb*/, u32 format, i32 fd, u32 size) noexcept
{
#if defined(GECKO_HAS_XKBCOMMON)
  if (format != WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1)
  {
    ::close(fd);
    return;
  }

  char* map = static_cast<char*>(::mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0));
  if (map == MAP_FAILED)
  {
    ::close(fd);
    return;
  }

  if (m_XkbState)
  {
    ::xkb_state_unref(m_XkbState);
    m_XkbState = nullptr;
  }
  if (m_XkbKeymap)
  {
    ::xkb_keymap_unref(m_XkbKeymap);
    m_XkbKeymap = nullptr;
  }

  m_XkbKeymap = ::xkb_keymap_new_from_string(m_XkbContext, map, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
  ::munmap(map, size);
  ::close(fd);

  if (m_XkbKeymap)
    m_XkbState = ::xkb_state_new(m_XkbKeymap);
#else
  (void)format;
  ::close(fd);
  (void)size;
#endif
}

void WaylandWindowsBackend::OnKeyboardEnter(::wl_keyboard* /*kb*/, u32 /*serial*/, ::wl_surface* surface,
                                            wl_array* /*keys*/) noexcept
{
  const u64 id = FindWindowBySurface(surface);
  m_FocusedKeyboard = id;

  if (id != WindowHandle::InvalidId)
  {
    m_Staged.push_back(MakeWaylandStagedEvent(events::WindowFocusChanged,
                                              events::WindowFocusChangedPayload {WindowHandle {id}, NowNsSafe(), 1}));
  }
}

void WaylandWindowsBackend::OnKeyboardLeave(::wl_keyboard* /*kb*/, u32 /*serial*/, ::wl_surface* surface) noexcept
{
  const u64 id = FindWindowBySurface(surface);
  m_FocusedKeyboard = WindowHandle::InvalidId;

  if (id != WindowHandle::InvalidId)
  {
    m_Staged.push_back(MakeWaylandStagedEvent(events::WindowFocusChanged,
                                              events::WindowFocusChangedPayload {WindowHandle {id}, NowNsSafe(), 0}));
  }
}

void WaylandWindowsBackend::OnKeyboardKey(::wl_keyboard* /*kb*/, u32 /*serial*/, u32 /*time*/, u32 key,
                                          u32 state) noexcept
{
  if (m_FocusedKeyboard == WindowHandle::InvalidId)
    return;

  const bool down = (state == WL_KEYBOARD_KEY_STATE_PRESSED);
  const u64 now = NowNsSafe();

#if defined(GECKO_HAS_XKBCOMMON)
  // evdev scancode -> xkb keycode (offset by 8)
  const xkb_keycode_t xkbCode = key + 8;
  const xkb_keysym_t sym = m_XkbState ? ::xkb_state_key_get_one_sym(m_XkbState, xkbCode) : XKB_KEY_NoSymbol;
  const KeyCode kc = XkbKeysymToKeyCode(sym);

  m_Staged.push_back(
      MakeWaylandStagedEvent(events::WindowKey, events::WindowKeyPayload {WindowHandle {m_FocusedKeyboard}, now, kc,
                                                                          down ? u8(1) : u8(0), 0}));

  if (down && m_XkbState)
  {
    const ::gecko::u32 cp = ::xkb_state_key_get_utf32(m_XkbState, xkbCode);
    // Skip C0 controls except tab/CR/LF, and DEL.
    const bool keep = cp != 0 && !(cp < 32 && cp != '\t' && cp != '\n' && cp != '\r') && cp != 127;
    if (keep)
    {
      m_Staged.push_back(MakeWaylandStagedEvent(events::WindowChar,
                                                events::WindowCharPayload {WindowHandle {m_FocusedKeyboard}, now, cp}));
    }
  }
#else
  (void)key;
  (void)down;
  (void)now;
#endif
}

void WaylandWindowsBackend::OnKeyboardModifiers(::wl_keyboard* /*kb*/, u32 /*serial*/, u32 modsDepressed,
                                                u32 modsLatched, u32 modsLocked, u32 group) noexcept
{
#if defined(GECKO_HAS_XKBCOMMON)
  if (m_XkbState)
  {
    ::xkb_state_update_mask(m_XkbState, modsDepressed, modsLatched, modsLocked, 0, 0, group);
  }
#else
  (void)modsDepressed;
  (void)modsLatched;
  (void)modsLocked;
  (void)group;
#endif
}

// -- Pointer callbacks --------------------------------------------------

void WaylandWindowsBackend::OnPointerEnter(::wl_pointer* /*pointer*/, u32 serial, ::wl_surface* surface,
                                           wl_fixed_t /*sx*/, wl_fixed_t /*sy*/) noexcept
{
  m_LastPointerSerial = serial;
  m_FocusedPointer = FindWindowBySurface(surface);

  // Apply cursor for the entered window.
  if (m_FocusedPointer != WindowHandle::InvalidId)
  {
    auto it = m_Windows.find(m_FocusedPointer);
    if (it != m_Windows.end())
      ApplyCursorVisibility(it->second);

    m_Staged.push_back(MakeWaylandStagedEvent(
        events::WindowMouseEntered, events::WindowMouseEnteredPayload {WindowHandle {m_FocusedPointer}, NowNsSafe()}));
  }
}

void WaylandWindowsBackend::OnPointerLeave(::wl_pointer* /*pointer*/, u32 /*serial*/,
                                           ::wl_surface* /*surface*/) noexcept
{
  if (m_FocusedPointer != WindowHandle::InvalidId)
  {
    m_Staged.push_back(MakeWaylandStagedEvent(
        events::WindowMouseExited, events::WindowMouseExitedPayload {WindowHandle {m_FocusedPointer}, NowNsSafe()}));
  }
  m_FocusedPointer = WindowHandle::InvalidId;
}

void WaylandWindowsBackend::OnPointerMotion(::wl_pointer* /*pointer*/, u32 /*time*/, wl_fixed_t sx,
                                            wl_fixed_t sy) noexcept
{
  if (m_FocusedPointer == WindowHandle::InvalidId)
    return;

  m_Staged.push_back(MakeWaylandStagedEvent(
      events::WindowMouseMove, events::WindowMouseMovePayload {WindowHandle {m_FocusedPointer}, NowNsSafe(),
                                                               wl_fixed_to_int(sx), wl_fixed_to_int(sy)}));
}

void WaylandWindowsBackend::OnPointerButton(::wl_pointer* /*pointer*/, u32 serial, u32 /*time*/, u32 button,
                                            u32 state) noexcept
{
  m_LastPointerSerial = serial;
  if (m_FocusedPointer == WindowHandle::InvalidId)
    return;

  const bool down = (state == WL_POINTER_BUTTON_STATE_PRESSED);
  m_Staged.push_back(MakeWaylandStagedEvent(
      events::WindowMouseButton,
      events::WindowMouseButtonPayload {WindowHandle {m_FocusedPointer}, NowNsSafe(),
                                        WaylandButtonToMouseButton(button), down ? u8(1) : u8(0)}));
}

void WaylandWindowsBackend::OnPointerAxis(::wl_pointer* /*pointer*/, u32 /*time*/, u32 axis, wl_fixed_t value) noexcept
{
  if (m_FocusedPointer == WindowHandle::InvalidId)
    return;

  float dx = 0.0F;
  float dy = 0.0F;
  const float v = static_cast<float>(wl_fixed_to_double(value));

  // WL_POINTER_AXIS_VERTICAL_SCROLL = 0, HORIZONTAL = 1
  if (axis == 0)
    dy = v > 0 ? -1.0F : 1.0F;
  else
    dx = v > 0 ? 1.0F : -1.0F;

  m_Staged.push_back(
      MakeWaylandStagedEvent(events::WindowMouseWheel,
                             events::WindowMouseWheelPayload {WindowHandle {m_FocusedPointer}, NowNsSafe(), dx, dy}));
}

// -- Helpers ------------------------------------------------------------

u64 WaylandWindowsBackend::FindWindowBySurface(::wl_surface* surface) const noexcept
{
  auto it = m_WindowBySurface.find(surface);
  if (it == m_WindowBySurface.end())
    return WindowHandle::InvalidId;
  return it->second;
}

u64 NowNsSafe() noexcept;

// -- Factory ------------------------------------------------------------

Unique<IWindowsBackend> CreateWaylandWindowsBackend() noexcept
{
  return CreateUnique<WaylandWindowsBackend>();
}

}  // namespace gecko::platform

#endif  // GECKO_PLATFORM_LINUX && GECKO_PLATFORM_LINUX_WAYLAND
