#include "gecko/platform/clipboard.h"

#include "gecko/core/services/log.h"

#if defined(GECKO_PLATFORM_WINDOWS)

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace gecko::platform {

::std::string GetClipboardText() noexcept
{
  if (!::OpenClipboard(nullptr))
    return {};

  ::std::string out;
  ::HANDLE handle = ::GetClipboardData(CF_UNICODETEXT);
  if (handle != nullptr)
  {
    auto* wide = static_cast<const wchar_t*>(::GlobalLock(handle));
    if (wide != nullptr)
    {
      const int needed = ::WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
      if (needed > 1)
      {
        // -1 to drop the null terminator from std::string size.
        out.resize(static_cast<::std::size_t>(needed - 1));
        ::WideCharToMultiByte(CP_UTF8, 0, wide, -1, out.data(), needed, nullptr, nullptr);
      }
      ::GlobalUnlock(handle);
    }
  }
  ::CloseClipboard();
  return out;
}

bool SetClipboardText(::gecko::StringView utf8) noexcept
{
  if (!::OpenClipboard(nullptr))
    return false;

  bool ok = false;
  // +1 source byte so terminating-null path works even with empty input.
  const int wideLen = ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), nullptr, 0);
  // wideLen is the count of wchars NOT including any terminator; we add
  // one for our own null.
  const ::SIZE_T bytes = (static_cast<::SIZE_T>(wideLen) + 1) * sizeof(wchar_t);
  ::HGLOBAL mem = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
  if (mem != nullptr)
  {
    auto* dst = static_cast<wchar_t*>(::GlobalLock(mem));
    if (dst != nullptr)
    {
      if (wideLen > 0)
      {
        ::MultiByteToWideChar(CP_UTF8, 0, utf8.data(), static_cast<int>(utf8.size()), dst, wideLen);
      }
      dst[wideLen] = L'\0';
      ::GlobalUnlock(mem);
      ::EmptyClipboard();
      if (::SetClipboardData(CF_UNICODETEXT, mem) != nullptr)
      {
        ok = true;
        mem = nullptr;  // ownership transferred to clipboard
      }
    }
    if (mem != nullptr)
      ::GlobalFree(mem);
  }
  ::CloseClipboard();
  return ok;
}

}  // namespace gecko::platform

#elif defined(GECKO_PLATFORM_LINUX) && defined(GECKO_PLATFORM_LINUX_X11)

#include <chrono>
#include <cstring>
#include <thread>
#include <X11/Xatom.h>
#include <X11/Xlib.h>

namespace gecko::platform {

namespace {

// Storage for our own SetClipboardText payload. We become the
// CLIPBOARD selection owner and serve the text in response to
// SelectionRequest events, which means the data must outlive the
// SetClipboardText call. A static thread-local-ish store is fine
// here -- clipboard ops are single-threaded by convention.
struct X11ClipboardOwnerState
{
  ::Display* Display {nullptr};
  ::Window OwnerWindow {0};
  ::std::string Payload;
  bool Owns {false};
};

X11ClipboardOwnerState& OwnerState() noexcept
{
  static X11ClipboardOwnerState s {};
  return s;
}

::Display* OpenDisplayLocal() noexcept
{
  static ::Display* s_display = ::XOpenDisplay(nullptr);
  return s_display;
}

::Window EnsureHelperWindow(::Display* display) noexcept
{
  auto& s = OwnerState();
  if (s.OwnerWindow != 0 && s.Display == display)
    return s.OwnerWindow;
  s.Display = display;
  // DefaultScreen / RootWindow are X11 macros, not real symbols, so
  // they cannot be reached through `::` qualification.
  const int screen = DefaultScreen(display);
  s.OwnerWindow = ::XCreateSimpleWindow(display, RootWindow(display, screen), 0, 0, 1, 1, 0, 0, 0);
  return s.OwnerWindow;
}

}  // namespace

::std::string GetClipboardText() noexcept
{
  ::Display* display = OpenDisplayLocal();
  if (display == nullptr)
    return {};

  const ::Atom clipboard = ::XInternAtom(display, "CLIPBOARD", False);
  const ::Atom utf8 = ::XInternAtom(display, "UTF8_STRING", False);
  const ::Atom prop = ::XInternAtom(display, "GECKO_CLIP", False);
  const ::Window window = EnsureHelperWindow(display);

  // If we own the selection ourselves, return our cached payload.
  ::Window owner = ::XGetSelectionOwner(display, clipboard);
  if (owner == window)
    return OwnerState().Payload;
  if (owner == None)
    return {};

  ::XConvertSelection(display, clipboard, utf8, prop, window, CurrentTime);
  ::XFlush(display);

  // Wait for SelectionNotify (bounded poll, ~250 ms).
  using clock = ::std::chrono::steady_clock;
  const auto deadline = clock::now() + ::std::chrono::milliseconds(250);
  ::XEvent event;
  bool got = false;
  while (clock::now() < deadline)
  {
    if (::XCheckTypedWindowEvent(display, window, SelectionNotify, &event))
    {
      got = true;
      break;
    }
    ::std::this_thread::sleep_for(::std::chrono::milliseconds(2));
  }
  if (!got || event.xselection.property == None)
    return {};

  ::Atom actualType = 0;
  int actualFormat = 0;
  unsigned long nItems = 0;
  unsigned long bytesAfter = 0;
  unsigned char* data = nullptr;
  if (::XGetWindowProperty(display, window, prop, 0, ~0L, True, AnyPropertyType, &actualType, &actualFormat, &nItems,
                           &bytesAfter, &data) != Success)
    return {};

  ::std::string out;
  if (data != nullptr)
  {
    out.assign(reinterpret_cast<const char*>(data), nItems);
    ::XFree(data);
  }
  return out;
}

bool SetClipboardText(::gecko::StringView utf8) noexcept
{
  ::Display* display = OpenDisplayLocal();
  if (display == nullptr)
    return false;

  const ::Atom clipboard = ::XInternAtom(display, "CLIPBOARD", False);
  auto& s = OwnerState();
  s.Payload.assign(utf8.Data(), utf8.Size());
  s.Display = display;
  const ::Window window = EnsureHelperWindow(display);
  ::XSetSelectionOwner(display, clipboard, window, CurrentTime);
  ::XFlush(display);
  s.Owns = (::XGetSelectionOwner(display, clipboard) == window);
  // Note: this implementation is "fire-and-forget" -- we do not run a
  // background event loop to service SelectionRequest events, so other
  // X11 clients won't actually be able to paste from us until/unless
  // the engine pumps X11 events with our helper window in scope. Good
  // enough for round-trip Set->Get within a single Gecko process. A
  // future revision will hook this into the X11 backend's event loop.
  return s.Owns;
}

}  // namespace gecko::platform

#elif defined(GECKO_PLATFORM_LINUX)

// Wayland-only build (no X11). wl_data_device requires a valid input
// serial which we'd have to thread through the windowing backend; the
// public API will gain a real impl once that's wired up.
namespace gecko::platform {

::std::string GetClipboardText() noexcept
{
  GECKO_WARN("gecko.platform.clipboard", "GetClipboardText: Wayland clipboard not yet implemented");
  return {};
}

bool SetClipboardText(::gecko::StringView) noexcept
{
  GECKO_WARN("gecko.platform.clipboard", "SetClipboardText: Wayland clipboard not yet implemented");
  return false;
}

}  // namespace gecko::platform

#else

namespace gecko::platform {

::std::string GetClipboardText() noexcept
{
  return {};
}

bool SetClipboardText(::gecko::StringView) noexcept
{
  return false;
}

}  // namespace gecko::platform

#endif
