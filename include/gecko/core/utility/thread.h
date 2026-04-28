#pragma once

/// @file
/// Thread-related helpers: sleeping, yielding, and identifying threads.

#include "gecko/core/api.h"
#include "gecko/core/types.h"

#include <chrono>
#include <thread>

namespace gecko {

/// @return A 32-bit hash of the calling thread's `std::thread::id`.
///         Stable for the lifetime of the thread.
GECKO_API u32 HashThreadId() noexcept;

/// @return Number of hardware threads reported by the OS, or `1` if
///         the runtime cannot determine it.
GECKO_API u32 HardwareThreadCount() noexcept;

/// Sleep the calling thread for at least `milliseconds`.
inline void SleepMs(u64 milliseconds) noexcept
{
  ::std::this_thread::sleep_for(::std::chrono::milliseconds(milliseconds));
}

/// Sleep the calling thread for at least `microseconds`.
inline void SleepUs(u64 microseconds) noexcept
{
  ::std::this_thread::sleep_for(::std::chrono::microseconds(microseconds));
}

/// Sleep the calling thread for at least `nanoseconds`.
inline void SleepNs(u64 nanoseconds) noexcept
{
  ::std::this_thread::sleep_for(::std::chrono::nanoseconds(nanoseconds));
}

/// Hint to the scheduler that the thread can give up its remaining
/// quantum.
inline void YieldThread() noexcept
{
  ::std::this_thread::yield();
}

/// Busy-wait for at least `nanoseconds` without releasing the core.
/// Use only for very short waits where `SleepNs` is too coarse.
GECKO_API void SpinWaitNs(u64 nanoseconds) noexcept;

/// Hybrid sleep that combines `SleepNs` with a short spin tail to land
/// closer to `nanoseconds` than the OS scheduler usually allows.
GECKO_API void PreciseSleepNs(u64 nanoseconds) noexcept;

}  // namespace gecko

#ifndef GECKO_THREADING
#define GECKO_THREADING 1
#endif

#if GECKO_THREADING
#define GECKO_SLEEP_MS(ms) ::gecko::SleepMs(ms)
#define GECKO_SLEEP_US(us) ::gecko::SleepUs(us)
#define GECKO_SLEEP_NS(ns) ::gecko::SleepNs(ns)
#define GECKO_YIELD() ::gecko::YieldThread()
#define GECKO_SPIN_WAIT_NS(ns) ::gecko::SpinWaitNs(ns)
#define GECKO_PRECISE_SLEEP_NS(ns) ::gecko::PreciseSleepNs(ns)
#define GECKO_THREAD_HASH() ::gecko::HashThreadId()
#else
#define GECKO_SLEEP_MS(ms)
#define GECKO_SLEEP_US(us)
#define GECKO_SLEEP_NS(ns)
#define GECKO_YIELD()
#define GECKO_SPIN_WAIT_NS(ns)
#define GECKO_PRECISE_SLEEP_NS(ns)
#define GECKO_THREAD_HASH() 0U
#endif
