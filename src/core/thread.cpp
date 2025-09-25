#include "gecko/core/thread.h"

#include <chrono>
#include <functional>
#include <thread>

#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
  #include <intrin.h>
#endif

#include "gecko/core/assert.h"
#include "gecko/core/time.h"

namespace gecko {

  u32 HashThreadId() noexcept
  {
    auto id = std::hash<std::thread::id>{}(std::this_thread::get_id());
    return static_cast<u32>(id ^ (id >> 32));
  }

  u32 HardwareThreadCount() noexcept
  {
    return std::max(1u, std::thread::hardware_concurrency());
  }

  void SpinWaitNs(u64 nanoseconds) noexcept
  {
    GECKO_ASSERT(nanoseconds > 0 && "Spin wait duration must be greater than 0");
    
    u64 start = HighResTimeNs();
    u64 target = start + nanoseconds;
    
    while (HighResTimeNs() < target)
    {
      // Busy wait - intentionally empty
      // On some architectures, you might want to add a pause instruction here
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
      _mm_pause(); // x86/x64 pause instruction to reduce power and improve performance
#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
      __builtin_ia32_pause();
#endif
    }
  }

  void PreciseSleepNs(u64 nanoseconds) noexcept
  {
    if (nanoseconds == 0) return;
    
    // For very short durations, just spin-wait
    if (nanoseconds < 1000) // Less than 1 microsecond
    {
      SpinWaitNs(nanoseconds);
      return;
    }
    
    // For longer durations, sleep for most of the time and spin-wait for the remainder
    const u64 spinThresholdNs = 100000; // 100 microseconds
    
    if (nanoseconds > spinThresholdNs)
    {
      u64 sleepTimeNs = nanoseconds - spinThresholdNs;
      std::this_thread::sleep_for(std::chrono::nanoseconds(sleepTimeNs));
      SpinWaitNs(spinThresholdNs);
    }
    else
    {
      SpinWaitNs(nanoseconds);
    }
  }

}
