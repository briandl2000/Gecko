#if defined(GECKO_PLATFORM_WINDOWS)

#include "../private/native_threading.h"
#include "gecko/core/ptr.h"
#include "gecko/platform/threading.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <processthreadsapi.h>
#include <Windows.h>

namespace gecko::platform {

namespace {

class Win32Threading final : public IThreading
{
public:
  Win32Threading() noexcept
  {
    ::SYSTEM_INFO si {};
    ::GetSystemInfo(&si);
    m_HardwareThreadCount =
        si.dwNumberOfProcessors > 0 ? si.dwNumberOfProcessors : 1u;
  }

  [[nodiscard]] ::gecko::u32 GetHardwareThreadCount() const noexcept override
  {
    return m_HardwareThreadCount;
  }

  [[nodiscard]] ThreadId GetCurrentThreadId() const noexcept override
  {
    return static_cast<ThreadId>(::GetCurrentThreadId());
  }

  void SetCurrentThreadName(const char* name) noexcept override
  {
    if (name == nullptr)
      return;

    // SetThreadDescription takes wide strings; convert ASCII / UTF-8 to
    // UTF-16 manually to avoid a runtime dependency on MultiByteToWideChar
    // semantics for the common ASCII case.
    wchar_t wide[128];
    ::std::size_t i = 0;
    for (; i < (sizeof(wide) / sizeof(wide[0])) - 1 && name[i] != '\0'; ++i)
      wide[i] = static_cast<wchar_t>(static_cast<unsigned char>(name[i]));
    wide[i] = L'\0';

    // SetThreadDescription is Windows 10+ but ships in kernel32 on all
    // supported targets for this engine; link-time resolution is fine.
    ::SetThreadDescription(::GetCurrentThread(), wide);
  }

  void SleepNanoseconds(::gecko::u64 nanoseconds) const noexcept override
  {
    if (nanoseconds == 0)
      return;
    // Convert to milliseconds, rounding up so we never sleep less than
    // requested. PlatformModule requests 1ms timer resolution at startup.
    ::gecko::u64 ms = (nanoseconds + 999'999ULL) / 1'000'000ULL;
    if (ms == 0)
      ms = 1;
    ::Sleep(static_cast<::DWORD>(ms));
  }

  void YieldThread() const noexcept override
  {
    ::SwitchToThread();
  }

private:
  ::gecko::u32 m_HardwareThreadCount {1};
};

}  // namespace

::gecko::Unique<IThreading> CreateNativeThreading() noexcept
{
  return ::gecko::CreateUnique<Win32Threading>();
}

}  // namespace gecko::platform

#endif  // GECKO_PLATFORM_WINDOWS
