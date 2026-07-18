#include "gecko/core/utility/thread.h"

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
#include <intrin.h>
#endif

#include "gecko/core/assert.h"
#include "gecko/core/utility/time.h"

namespace gecko {

u32 HashThreadId() noexcept
{
  const u64 id = platform::CurrentThreadId();
  return static_cast<u32>(id ^ (id >> 32U));
}

u32 ThisThreadId() noexcept
{
  return HashThreadId();
}

u32 HardwareThreadCount() noexcept
{
  return platform::HardwareThreadCount();
}

void SpinWaitNs(u64 nanoseconds) noexcept
{
  GECKO_ASSERT(nanoseconds > 0 && "Spin wait duration must be greater than 0");

  u64 start = HighResTimeNs();
  u64 target = start + nanoseconds;

  while (HighResTimeNs() < target)
  {
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
    _mm_pause();
#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
    __builtin_ia32_pause();
#endif
  }
}

void PreciseSleepNs(u64 nanoseconds) noexcept
{
  if (nanoseconds == 0)
    return;

  if (nanoseconds < 1000)
  {
    SpinWaitNs(nanoseconds);
    return;
  }

  // On Windows the default timer resolution is ~15.6ms, so short sleeps
  // overshoot massively.  Use a larger spin threshold to compensate.
#if defined(GECKO_PLATFORM_WINDOWS)
  const u64 spinThresholdNs = 2000000;  // 2ms -- stay within one timer tick
#else
  const u64 spinThresholdNs = 100000;  // 100us -- Linux/macOS are fine
#endif

  if (nanoseconds > spinThresholdNs)
  {
    u64 sleepTimeNs = nanoseconds - spinThresholdNs;
    platform::SleepNanoseconds(sleepTimeNs);
    SpinWaitNs(spinThresholdNs);
  }
  else
  {
    SpinWaitNs(nanoseconds);
  }
}

}  // namespace gecko
