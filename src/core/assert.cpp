#include "gecko/core/assert.h"

#include "gecko/core/format.h"

#if defined(GECKO_PLATFORM_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#elif defined(GECKO_PLATFORM_LINUX)
#include <signal.h>
#include <unistd.h>
#endif

namespace gecko {

[[noreturn]] void AssertFailure(const AssertInfo& info) noexcept
{
  char text[2048] {};
  FormatBuffer output {.Data = text, .Capacity = sizeof(text)};
  const FormatArg arguments[] {
      MakeFormatArg(info.Expression != nullptr ? info.Expression : "<none>"),
      MakeFormatArg(info.Message != nullptr ? info.Message : ""),
      MakeFormatArg(info.File != nullptr ? info.File : "<unknown>"),
      MakeFormatArg(info.Line),
      MakeFormatArg(info.Function != nullptr ? info.Function : "<unknown>"),
  };
  FormatTo(output,
           "\nGECKO ASSERTION FAILED\n  expression: {}\n  message:    {}\n  location:   {}:{}\n  function:   {}\n",
           Span<const FormatArg> {arguments, 5});
  const usize length = output.Length < sizeof(text) ? output.Length : sizeof(text) - 1U;

#if defined(GECKO_PLATFORM_WINDOWS)
  ::OutputDebugStringA(text);
  HANDLE errorOutput = ::GetStdHandle(STD_ERROR_HANDLE);
  if (errorOutput != nullptr && errorOutput != INVALID_HANDLE_VALUE)
  {
    DWORD written = 0;
    (void)::WriteFile(errorOutput, text, static_cast<DWORD>(length), &written, nullptr);
  }
  if (::IsDebuggerPresent())
    ::DebugBreak();
  ::TerminateProcess(::GetCurrentProcess(), 3);
#elif defined(GECKO_PLATFORM_LINUX)
  usize written = 0;
  while (written < length)
  {
    const ssize_t result = ::write(STDERR_FILENO, text + written, length - written);
    if (result <= 0)
      break;
    written += static_cast<usize>(result);
  }
  ::raise(SIGTRAP);
  _exit(3);
#else
  __builtin_trap();
#endif
}

}  // namespace gecko
