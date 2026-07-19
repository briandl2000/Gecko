#if defined(GECKO_PLATFORM_LINUX)

#include "threading_linux.h"

#include "gecko/platform/threading.h"

#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <sys/syscall.h>
#include <time.h>
#include <unistd.h>

namespace gecko::platform {

gecko::u32 HardwareThreadCount() noexcept
{
  long n = ::sysconf(_SC_NPROCESSORS_ONLN);
  return (n > 0) ? static_cast<gecko::u32>(n) : 1u;
}

ThreadId CurrentThreadId() noexcept
{
  return static_cast<ThreadId>(::syscall(SYS_gettid));
}

void SetCurrentThreadName(const char* name) noexcept
{
  if (name == nullptr)
    return;
  // pthread_setname_np caps at 16 chars including the trailing NUL.
  char buf[16];
  usize i = 0;
  for (; i < sizeof(buf) - 1 && name[i] != '\0'; ++i)
    buf[i] = name[i];
  buf[i] = '\0';
  ::pthread_setname_np(::pthread_self(), buf);
}

void SleepNanoseconds(gecko::u64 nanoseconds) noexcept
{
  if (nanoseconds == 0)
    return;
  ::timespec ts {};
  ts.tv_sec = static_cast<::time_t>(nanoseconds / 1'000'000'000ULL);
  ts.tv_nsec = static_cast<long>(nanoseconds % 1'000'000'000ULL);
  while (::clock_nanosleep(CLOCK_MONOTONIC, 0, &ts, &ts) == EINTR)
  {}
}

void YieldThread() noexcept
{
  ::sched_yield();
}

}  // namespace gecko::platform

#endif  // GECKO_PLATFORM_LINUX
