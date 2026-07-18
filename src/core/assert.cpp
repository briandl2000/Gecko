#include "gecko/core/assert.h"

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

namespace {

struct TextBuffer
{
  char Data[2048] {};
  usize Length {0};
};

void Append(TextBuffer& output, const char* text) noexcept
{
  if (text == nullptr)
    return;
  while (*text != '\0' && output.Length + 1 < sizeof(output.Data))
    output.Data[output.Length++] = *text++;
}

void AppendNumber(TextBuffer& output, u32 value) noexcept
{
  char digits[16] {};
  usize count = 0;
  do
  {
    digits[count++] = static_cast<char>('0' + value % 10U);
    value /= 10U;
  } while (value != 0 && count < sizeof(digits));

  while (count != 0 && output.Length + 1 < sizeof(output.Data))
    output.Data[output.Length++] = digits[--count];
}

}  // namespace

[[noreturn]] void AssertFailure(const AssertInfo& info) noexcept
{
  TextBuffer text {};
  Append(text, "\nGECKO ASSERTION FAILED\n  expression: ");
  Append(text, info.Expression != nullptr ? info.Expression : "<none>");
  if (info.Message != nullptr && info.Message[0] != '\0')
  {
    Append(text, "\n  message:    ");
    Append(text, info.Message);
  }
  Append(text, "\n  location:   ");
  Append(text, info.File != nullptr ? info.File : "<unknown>");
  Append(text, ":");
  AppendNumber(text, info.Line);
  Append(text, "\n  function:   ");
  Append(text, info.Function != nullptr ? info.Function : "<unknown>");
  Append(text, "\n");
  text.Data[text.Length] = '\0';

#if defined(GECKO_PLATFORM_WINDOWS)
  ::OutputDebugStringA(text.Data);
  HANDLE errorOutput = ::GetStdHandle(STD_ERROR_HANDLE);
  if (errorOutput != nullptr && errorOutput != INVALID_HANDLE_VALUE)
  {
    DWORD written = 0;
    (void)::WriteFile(errorOutput, text.Data, static_cast<DWORD>(text.Length), &written, nullptr);
  }
  if (::IsDebuggerPresent())
    ::DebugBreak();
  ::TerminateProcess(::GetCurrentProcess(), 3);
#elif defined(GECKO_PLATFORM_LINUX)
  usize written = 0;
  while (written < text.Length)
  {
    const ssize_t result = ::write(STDERR_FILENO, text.Data + written, text.Length - written);
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
