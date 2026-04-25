#pragma once

#include "gecko/core/api.h"
#include "gecko/core/types.h"

#include <string_view>

namespace gecko::platform {

// Terminal text colors. Mirror the ANSI 8-color palette plus
// "Default" which leaves the terminal default in place. Bright
// variants use the "bold" / "1;3x" ANSI sequence on POSIX and
// the FOREGROUND_INTENSITY bit on Win32.
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

enum class TermStream : ::gecko::u8
{
  Stdout,
  Stderr,
};

// Write UTF-8 text to the given standard stream, optionally colorized.
//
// On Win32, the console code page is forced to CP_UTF8 once on first
// use so that multi-byte UTF-8 input is rendered correctly. Console
// virtual-terminal mode is enabled where supported so the same ANSI
// escape sequences work cross-platform; on older Windows hosts where
// VT mode cannot be enabled, the color attributes are applied via
// SetConsoleTextAttribute as a fallback.
//
// When the target stream is not a TTY (e.g. redirected to a file or
// piped), color is suppressed entirely so logs and tools never get
// raw escape codes in their captured output.
GECKO_API void Print(TermStream stream, TermColor fg,
                     ::std::string_view text) noexcept;

// Convenience: write text + newline.
GECKO_API void PrintLine(TermStream stream, TermColor fg,
                         ::std::string_view text) noexcept;

// Default-color writes.
inline void Print(TermStream stream, ::std::string_view text) noexcept
{
  Print(stream, TermColor::Default, text);
}
inline void PrintLine(TermStream stream, ::std::string_view text) noexcept
{
  PrintLine(stream, TermColor::Default, text);
}

// Returns true if the given stream looks like an interactive
// terminal (will receive color escapes from Print).
[[nodiscard]] GECKO_API bool IsTerminal(TermStream stream) noexcept;

}  // namespace gecko::platform
