#include "gecko/platform/terminal.h"

#if defined(GECKO_PLATFORM_WINDOWS)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#elif defined(GECKO_PLATFORM_LINUX)
#include <unistd.h>
#endif

namespace gecko::platform {

namespace {

const char* AnsiForeground(TermColor color) noexcept
{
  switch (color)
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

#if defined(GECKO_PLATFORM_WINDOWS)

HANDLE StreamHandle(TermStream stream) noexcept
{
  return ::GetStdHandle(stream == TermStream::Stderr ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);
}

void ConfigureWindowsConsole() noexcept
{
  static bool configured = false;
  if (configured)
    return;
  configured = true;
  (void)::SetConsoleOutputCP(CP_UTF8);
  constexpr DWORD streams[] {STD_OUTPUT_HANDLE, STD_ERROR_HANDLE};
  for (DWORD stream : streams)
  {
    HANDLE handle = ::GetStdHandle(stream);
    DWORD mode = 0;
    if (handle != nullptr && handle != INVALID_HANDLE_VALUE && ::GetConsoleMode(handle, &mode))
      (void)::SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
  }
}

void WriteRaw(TermStream stream, StringView text) noexcept
{
  HANDLE handle = StreamHandle(stream);
  if (handle == nullptr || handle == INVALID_HANDLE_VALUE || text.Empty())
    return;
  usize offset = 0;
  while (offset < text.Size())
  {
    DWORD written = 0;
    const DWORD count = static_cast<DWORD>(text.Size() - offset > 0xFFFFFFFFULL ? 0xFFFFFFFFULL : text.Size() - offset);
    if (!::WriteFile(handle, text.Data() + offset, count, &written, nullptr) || written == 0)
      break;
    offset += written;
  }
}

#elif defined(GECKO_PLATFORM_LINUX)

int StreamDescriptor(TermStream stream) noexcept
{
  return stream == TermStream::Stderr ? STDERR_FILENO : STDOUT_FILENO;
}

void WriteRaw(TermStream stream, StringView text) noexcept
{
  usize offset = 0;
  while (offset < text.Size())
  {
    const ssize_t written = ::write(StreamDescriptor(stream), text.Data() + offset, text.Size() - offset);
    if (written <= 0)
      break;
    offset += static_cast<usize>(written);
  }
}

#endif

}  // namespace

bool IsTerminal(TermStream stream) noexcept
{
#if defined(GECKO_PLATFORM_WINDOWS)
  ConfigureWindowsConsole();
  DWORD mode = 0;
  const HANDLE handle = StreamHandle(stream);
  return handle != nullptr && handle != INVALID_HANDLE_VALUE && ::GetConsoleMode(handle, &mode);
#elif defined(GECKO_PLATFORM_LINUX)
  return ::isatty(StreamDescriptor(stream)) != 0;
#endif
}

void Print(TermStream stream, TermColor foreground, StringView text) noexcept
{
#if defined(GECKO_PLATFORM_WINDOWS)
  ConfigureWindowsConsole();
#endif
  const bool colored = foreground != TermColor::Default && IsTerminal(stream);
  if (colored)
    WriteRaw(stream, StringView {AnsiForeground(foreground)});
  WriteRaw(stream, text);
  if (colored)
    WriteRaw(stream, StringView {"\x1b[0m"});
}

void PrintLine(TermStream stream, TermColor foreground, StringView text) noexcept
{
  Print(stream, foreground, text);
  WriteRaw(stream, StringView {"\n"});
}

}  // namespace gecko::platform
