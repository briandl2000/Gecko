#pragma once

/// @file
/// System clipboard text I/O.
///
/// All functions operate on the OS-level clipboard (`CLIPBOARD` on X11,
/// `wl_data_device` on Wayland, `CF_UNICODETEXT` on Win32). Text is
/// UTF-8 in both directions.
///
/// These are simple synchronous calls -- they may block briefly while
/// the windowing system completes the request, but never wait on user
/// interaction. Returning an empty string from `GetClipboardText()` is
/// a soft failure: the clipboard is empty, contains non-text data, or
/// the platform backend does not support it.
///
/// Backends:
/// - **Win32**: `SetClipboardData` / `GetClipboardData` with
///   `CF_UNICODETEXT`, converted to/from UTF-8 via `MultiByteToWideChar`.
/// - **X11**: `XConvertSelection` + ICCCM round-trip on a hidden window.
/// - **Wayland**: not yet implemented (needs a valid serial); returns
///   empty / no-op.

#include "gecko/core/api.h"

#include <string>
#include <string_view>

namespace gecko::platform {

/// Read UTF-8 text from the system clipboard.
/// @return The clipboard text, or an empty string on failure / non-text data.
[[nodiscard]] GECKO_API ::std::string GetClipboardText() noexcept;

/// Place UTF-8 text on the system clipboard.
/// @param utf8  The text to copy.
/// @return `true` on success, `false` on backend failure or when writes
///         are unsupported (e.g. current Wayland backend).
GECKO_API bool SetClipboardText(::std::string_view utf8) noexcept;

}  // namespace gecko::platform
