#if defined(GECKO_PLATFORM_WINDOWS)

#include "threading_win32.h"

#include "gecko/platform/threading.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <cstddef>
#include <processthreadsapi.h>
#include <Windows.h>

namespace gecko::platform {

::gecko::u32 HardwareThreadCount() noexcept
{
  ::SYSTEM_INFO si {};
  ::GetSystemInfo(&si);
  return si.dwNumberOfProcessors > 0 ? si.dwNumberOfProcessors : 1u;
}

ThreadId CurrentThreadId() noexcept
{
  return static_cast<ThreadId>(::GetCurrentThreadId());
}

void SetCurrentThreadName(const char* name) noexcept
{
  if (name == nullptr)
    return;
  wchar_t wide[128];
  ::std::size_t i = 0;
  for (; i < (sizeof(wide) / sizeof(wide[0])) - 1 && name[i] != '\0'; ++i)
    wide[i] = static_cast<wchar_t>(static_cast<unsigned char>(name[i]));
  wide[i] = L'\0';
  ::SetThreadDescription(::GetCurrentThread(), wide);
}

void SleepNanoseconds(::gecko::u64 nanoseconds) noexcept
{
  if (nanoseconds == 0)
    return;
  ::gecko::u64 ms = (nanoseconds + 999'999ULL) / 1'000'000ULL;
  if (ms == 0)
    ms = 1;
  ::Sleep(static_cast<::DWORD>(ms));
}

void YieldThread() noexcept
{
  ::SwitchToThread();
}

}  // namespace gecko::platform

#endif  // GECKO_PLATFORM_WINDOWS
