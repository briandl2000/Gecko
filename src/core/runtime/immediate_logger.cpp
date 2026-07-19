#include "private/immediate_logger.h"

#include "gecko/core/assert.h"

#if defined(GECKO_PLATFORM_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#elif defined(GECKO_PLATFORM_LINUX)
#include <unistd.h>
#endif

namespace gecko::runtime {

namespace {

thread_local bool g_LogActive = false;

const char* LevelName(LogLevel level) noexcept
{
  switch (level)
  {
  case LogLevel::Trace:
    return "TRACE";
  case LogLevel::Debug:
    return "DEBUG";
  case LogLevel::Info:
    return "INFO";
  case LogLevel::Warn:
    return "WARN";
  case LogLevel::Error:
    return "ERROR";
  case LogLevel::Fatal:
    return "FATAL";
  }
  return "?";
}

void Append(char* output, usize capacity, usize& length, const char* text) noexcept
{
  if (text == nullptr)
    return;
  while (*text != '\0' && length + 1U < capacity)
    output[length++] = *text++;
}

void WriteLog(bool error, const char* text, usize size) noexcept
{
#if defined(GECKO_PLATFORM_WINDOWS)
  HANDLE handle = ::GetStdHandle(error ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);
  if (handle != nullptr && handle != INVALID_HANDLE_VALUE)
  {
    DWORD written = 0;
    (void)::WriteFile(handle, text, static_cast<DWORD>(size), &written, nullptr);
  }
  ::OutputDebugStringA(text);
#elif defined(GECKO_PLATFORM_LINUX)
  const int descriptor = error ? STDERR_FILENO : STDOUT_FILENO;
  usize written = 0;
  while (written < size)
  {
    const ssize_t result = ::write(descriptor, text + written, size - written);
    if (result <= 0)
      break;
    written += static_cast<usize>(result);
  }
#endif
}

}  // namespace

void ImmediateLogger::LogFormatted(LogLevel level, Label label, const char* format,
                                   Span<const FormatArg> arguments) noexcept
{
  if (static_cast<u8>(level) < static_cast<u8>(m_Level))
    return;
  GECKO_ASSERT(format != nullptr, "Log format cannot be null");
  if (g_LogActive)
    return;
  g_LogActive = true;

#if defined(_MSC_VER)
  while (_InterlockedExchange(reinterpret_cast<volatile long*>(&m_Lock), 1) != 0)
  {}
#else
  while (__atomic_exchange_n(&m_Lock, 1U, __ATOMIC_ACQUIRE) != 0)
  {}
#endif

  char output[2048] {};
  usize length = 0;
  Append(output, sizeof(output), length, "[");
  Append(output, sizeof(output), length, LevelName(level));
  Append(output, sizeof(output), length, "][");
  Append(output, sizeof(output), length, label.Name != nullptr ? label.Name : "unlabeled");
  Append(output, sizeof(output), length, "] ");

  FormatBuffer formatBuffer {
      .Data = output,
      .Capacity = sizeof(output),
      .Length = length,
  };
  FormatTo(formatBuffer, format, arguments);
  length = formatBuffer.Length < sizeof(output) ? formatBuffer.Length : sizeof(output) - 1U;
  if (length + 1U < sizeof(output))
    output[length++] = '\n';
  output[length] = '\0';

  WriteLog(level >= LogLevel::Warn, output, length);

#if defined(_MSC_VER)
  (void)_InterlockedExchange(reinterpret_cast<volatile long*>(&m_Lock), 0);
#else
  __atomic_store_n(&m_Lock, 0U, __ATOMIC_RELEASE);
#endif
  g_LogActive = false;
}

}  // namespace gecko::runtime
