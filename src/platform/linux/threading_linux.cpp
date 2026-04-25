#if defined(GECKO_PLATFORM_LINUX)

#include "../private/native_threading.h"
#include "gecko/core/ptr.h"
#include "gecko/platform/threading.h"

#include <pthread.h>
#include <sched.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

namespace gecko::platform {

namespace {

class LinuxThreading final : public IThreading
{
public:
  LinuxThreading() noexcept
  {
    long n = ::sysconf(_SC_NPROCESSORS_ONLN);
    m_HardwareThreadCount = (n > 0) ? static_cast<::gecko::u32>(n) : 1u;
  }

  [[nodiscard]] ::gecko::u32 GetHardwareThreadCount() const noexcept override
  {
    return m_HardwareThreadCount;
  }

  [[nodiscard]] ThreadId GetCurrentThreadId() const noexcept override
  {
    // gettid() returns the kernel TID; matches /proc/<pid>/task entries
    // and gdb's "info threads" output.
    return static_cast<ThreadId>(::syscall(SYS_gettid));
  }

  void SetCurrentThreadName(const char* name) noexcept override
  {
    if (name == nullptr)
      return;
    // pthread_setname_np caps at 16 chars including the trailing NUL on
    // Linux. Truncate manually so over-long names still set a useful
    // prefix instead of failing silently.
    char buf[16];
    ::std::size_t i = 0;
    for (; i < sizeof(buf) - 1 && name[i] != '\0'; ++i)
      buf[i] = name[i];
    buf[i] = '\0';
    ::pthread_setname_np(::pthread_self(), buf);
  }

  void SleepNanoseconds(::gecko::u64 nanoseconds) const noexcept override
  {
    if (nanoseconds == 0)
      return;
    ::timespec ts {};
    ts.tv_sec = static_cast<::time_t>(nanoseconds / 1'000'000'000ULL);
    ts.tv_nsec = static_cast<long>(nanoseconds % 1'000'000'000ULL);
    // Loop on EINTR so signal delivery doesn't shorten the sleep.
    while (::clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, &ts) == EINTR)
    {}
  }

  void YieldThread() const noexcept override
  {
    ::sched_yield();
  }

private:
  ::gecko::u32 m_HardwareThreadCount {1};
};

}  // namespace

::gecko::Unique<IThreading> CreateNativeThreading() noexcept
{
  return ::gecko::CreateUnique<LinuxThreading>();
}

}  // namespace gecko::platform

#endif  // GECKO_PLATFORM_LINUX
