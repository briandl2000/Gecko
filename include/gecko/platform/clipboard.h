#pragma once

#include "gecko/core/api.h"

#include <string>
#include <string_view>

namespace gecko::platform {

// System clipboard text I/O.
//
// All functions operate on the OS-level "selection clipboard"
// (CLIPBOARD on X11, the standard wl_data_device on Wayland, the
// WM_CLIPBOARD on Win32). Text is UTF-8 in both directions.
//
// These are simple synchronous calls — they may block briefly while
// the windowing system completes the request, but should never wait
// on user interaction. Returning an empty string from
// GetClipboardText() is a soft-failure: clipboard is empty, contains
// non-text data, or the platform backend doesn't support it.
//
// Backends:
//   * Win32: SetClipboardData / GetClipboardData with CF_UNICODETEXT,
//     converted to/from UTF-8 via MultiByteToWideChar.
//   * X11:   XConvertSelection + ICCCM round-trip on a hidden window.
//   * Wayland: not yet implemented (needs wl_data_device with a
//     valid serial). Returns empty / no-op until then.

[[nodiscard]] GECKO_API ::std::string GetClipboardText() noexcept;

// Returns true when the platform reports the text was successfully
// placed on the clipboard. Returns false on backend failure or when
// the backend does not support clipboard writes (e.g. Wayland).
GECKO_API bool SetClipboardText(::std::string_view utf8) noexcept;

}  // namespace gecko::platform
