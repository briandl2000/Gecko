#pragma once

#include "api.h"
#include "types.h"

#include <chrono>
#include <thread>

namespace gecko {

// Note: ThisThreadId() is defined in profiler.h

// Convert thread::id to a u32 hash for consistent thread identification
GECKO_API u32 HashThreadId() noexcept;

// Get current thread's hardware thread count
GECKO_API u32 HardwareThreadCount() noexcept;

// Sleep for specified duration
GECKO_API inline void SleepMs(u32 milliseconds) noexcept
{
  std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

GECKO_API inline void SleepUs(u32 microseconds) noexcept
{
  std::this_thread::sleep_for(std::chrono::microseconds(microseconds));
}

GECKO_API inline void SleepNs(u64 nanoseconds) noexcept
{
  std::this_thread::sleep_for(std::chrono::nanoseconds(nanoseconds));
}

// Yield current thread's time slice
GECKO_API inline void YieldThread() noexcept
{
  std::this_thread::yield();
}

// Spin-wait for a short duration (busy wait)
GECKO_API void SpinWaitNs(u64 nanoseconds) noexcept;

// High-resolution sleep that tries to be more accurate than
// std::this_thread::sleep_for Uses a combination of sleep and spin-wait for
// better precision
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
