#pragma once

/// @file
/// Color-aware writes to standard output / standard error streams.
///
/// `Print` / `PrintLine` write UTF-8 text with optional ANSI colour
/// support. On Win32 the console code page is forced to `CP_UTF8` and
/// virtual-terminal mode is enabled (Windows 10+); colour is suppressed
/// automatically when the destination is not a TTY.

#include "gecko/core/api.h"
#include "gecko/core/types.h"

#include <string_view>

namespace gecko::platform {

/// Terminal text colour, mirroring the ANSI 8-colour palette plus a
/// `Default` value (leave terminal default in place). Bright variants
/// use the bold / `1;3x` ANSI sequence on POSIX and the
/// `FOREGROUND_INTENSITY` bit on Win32.
enum class TermColor : ::gecko::u8
{
  Default,
  Black,
  Red,
  Green,
  Yellow,
  Blue,
  Magenta,
  Cyan,
  White,
  BrightBlack,
  BrightRed,
  BrightGreen,
  BrightYellow,
  BrightBlue,
  BrightMagenta,
  BrightCyan,
  BrightWhite,
};

/// Standard stream selector for `Print` / `PrintLine`.
enum class TermStream : ::gecko::u8
{
  Stdout,  ///< Standard output.
  Stderr,  ///< Standard error.
};

/// Write UTF-8 `text` to `stream`, optionally colorized with `fg`.
///
/// On Win32 the console code page is set to `CP_UTF8` once at first use
/// and virtual-terminal mode is enabled where supported. When `stream`
/// is not a TTY (redirection or pipe) colour is suppressed so captured
/// output never contains raw escape sequences.
GECKO_API void Print(TermStream stream, TermColor fg, ::std::string_view text) noexcept;

/// Convenience: write `text` followed by a newline.
GECKO_API void PrintLine(TermStream stream, TermColor fg, ::std::string_view text) noexcept;

/// Default-colour overload.
inline void Print(TermStream stream, ::std::string_view text) noexcept
{
  Print(stream, TermColor::Default, text);
}
/// Default-colour overload.
inline void PrintLine(TermStream stream, ::std::string_view text) noexcept
{
  PrintLine(stream, TermColor::Default, text);
}

/// `true` if `stream` looks like an interactive terminal (i.e. will
/// receive colour escapes from `Print`).
[[nodiscard]] GECKO_API bool IsTerminal(TermStream stream) noexcept;

}  // namespace gecko::platform
