#pragma once

#include "gecko/core/api.h"
#include "gecko/core/types.h"

#include <chrono>
#include <thread>

namespace gecko {

GECKO_API u32 HashThreadId() noexcept;

GECKO_API u32 HardwareThreadCount() noexcept;

inline void SleepMs(u64 milliseconds) noexcept
{
  ::std::this_thread::sleep_for(::std::chrono::milliseconds(milliseconds));
}

inline void SleepUs(u64 microseconds) noexcept
{
  ::std::this_thread::sleep_for(::std::chrono::microseconds(microseconds));
}

inline void SleepNs(u64 nanoseconds) noexcept
{
  ::std::this_thread::sleep_for(::std::chrono::nanoseconds(nanoseconds));
}

inline void YieldThread() noexcept
{
  ::std::this_thread::yield();
}

GECKO_API void SpinWaitNs(u64 nanoseconds) noexcept;

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
