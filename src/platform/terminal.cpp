#include "gecko/platform/terminal.h"

#include <cstdio>

#if defined(GECKO_PLATFORM_WINDOWS)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace gecko::platform {

namespace {

[[nodiscard]] ::std::FILE* StreamFile(TermStream s) noexcept
{
  return s == TermStream::Stderr ? stderr : stdout;
}

[[nodiscard]] const char* AnsiFg(TermColor c) noexcept
{
  switch (c)
  {
  case TermColor::Default:
    return "\x1b[0m";
  case TermColor::Black:
    return "\x1b[30m";
  case TermColor::Red:
    return "\x1b[31m";
  case TermColor::Green:
    return "\x1b[32m";
  case TermColor::Yellow:
    return "\x1b[33m";
  case TermColor::Blue:
    return "\x1b[34m";
  case TermColor::Magenta:
    return "\x1b[35m";
  case TermColor::Cyan:
    return "\x1b[36m";
  case TermColor::White:
    return "\x1b[37m";
  case TermColor::BrightBlack:
    return "\x1b[90m";
  case TermColor::BrightRed:
    return "\x1b[91m";
  case TermColor::BrightGreen:
    return "\x1b[92m";
  case TermColor::BrightYellow:
    return "\x1b[93m";
  case TermColor::BrightBlue:
    return "\x1b[94m";
  case TermColor::BrightMagenta:
    return "\x1b[95m";
  case TermColor::BrightCyan:
    return "\x1b[96m";
  case TermColor::BrightWhite:
    return "\x1b[97m";
  }
  return "\x1b[0m";
}

constexpr const char* kAnsiReset = "\x1b[0m";

#if defined(GECKO_PLATFORM_WINDOWS)

// Enable VT processing + force CP_UTF8 once. Done lazily on the first
// Print() call so we don't reach into the console for headless tools
// that never write to it.
void EnsureWindowsConsoleConfigured() noexcept
{
  static const bool s_done = []() noexcept {
    // Force the console output to interpret bytes as UTF-8. Critical
    // for non-ASCII log output and clipboard-style messages.
    ::SetConsoleOutputCP(CP_UTF8);

    auto enableVt = [](::DWORD which) noexcept {
      ::HANDLE h = ::GetStdHandle(which);
      if (h == INVALID_HANDLE_VALUE || h == nullptr)
        return;
      ::DWORD mode = 0;
      if (!::GetConsoleMode(h, &mode))
        return;
      ::SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    };
    enableVt(STD_OUTPUT_HANDLE);
    enableVt(STD_ERROR_HANDLE);
    return true;
  }();
  (void)s_done;
}

[[nodiscard]] bool StreamIsTtyImpl(TermStream s) noexcept
{
  const int fd = ::_fileno(StreamFile(s));
  if (fd < 0)
    return false;
  return ::_isatty(fd) != 0;
}

#else

[[nodiscard]] bool StreamIsTtyImpl(TermStream s) noexcept
{
  const int fd = ::fileno(StreamFile(s));
  if (fd < 0)
    return false;
  return ::isatty(fd) != 0;
}

#endif

void WriteUtf8(::std::FILE* f, ::std::string_view text) noexcept
{
  if (text.empty())
    return;
  ::std::fwrite(text.data(), 1, text.size(), f);
}

}  // namespace

bool IsTerminal(TermStream stream) noexcept
{
#if defined(GECKO_PLATFORM_WINDOWS)
  EnsureWindowsConsoleConfigured();
#endif
  return StreamIsTtyImpl(stream);
}

void Print(TermStream stream, TermColor fg, ::std::string_view text) noexcept
{
#if defined(GECKO_PLATFORM_WINDOWS)
  EnsureWindowsConsoleConfigured();
#endif
  ::std::FILE* f = StreamFile(stream);

  const bool tty = StreamIsTtyImpl(stream);
  const bool color = tty && fg != TermColor::Default;

  if (color)
    ::std::fputs(AnsiFg(fg), f);
  WriteUtf8(f, text);
  if (color)
    ::std::fputs(kAnsiReset, f);

  ::std::fflush(f);
}

void PrintLine(TermStream stream, TermColor fg, ::std::string_view text) noexcept
{
  Print(stream, fg, text);
  ::std::fputc('\n', StreamFile(stream));
  ::std::fflush(StreamFile(stream));
}

}  // namespace gecko::platform
