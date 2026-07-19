#include "gecko/core/utility/time.h"

#if defined(GECKO_PLATFORM_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#elif defined(GECKO_PLATFORM_LINUX)
#include <time.h>
#endif

namespace gecko {

u64 MonotonicTimeNs() noexcept
{
#if defined(GECKO_PLATFORM_WINDOWS)
  LARGE_INTEGER counter {};
  LARGE_INTEGER frequency {};
  (void)::QueryPerformanceCounter(&counter);
  (void)::QueryPerformanceFrequency(&frequency);
  return static_cast<u64>((static_cast<unsigned long long>(counter.QuadPart) * 1'000'000'000ULL) /
                          static_cast<unsigned long long>(frequency.QuadPart));
#elif defined(GECKO_PLATFORM_LINUX)
  timespec value {};
  (void)::clock_gettime(CLOCK_MONOTONIC, &value);
  return static_cast<u64>(value.tv_sec) * 1'000'000'000ULL + static_cast<u64>(value.tv_nsec);
#endif
}

u64 HighResTimeNs() noexcept
{
  return MonotonicTimeNs();
}

u64 SystemTimeNs() noexcept
{
#if defined(GECKO_PLATFORM_WINDOWS)
  FILETIME fileTime {};
  ::GetSystemTimePreciseAsFileTime(&fileTime);
  ULARGE_INTEGER value {};
  value.LowPart = fileTime.dwLowDateTime;
  value.HighPart = fileTime.dwHighDateTime;
  constexpr u64 FileTimeUnixDelta = 116444736000000000ULL;
  return (value.QuadPart - FileTimeUnixDelta) * 100ULL;
#elif defined(GECKO_PLATFORM_LINUX)
  timespec value {};
  (void)::clock_gettime(CLOCK_REALTIME, &value);
  return static_cast<u64>(value.tv_sec) * 1'000'000'000ULL + static_cast<u64>(value.tv_nsec);
#endif
}

}  // namespace gecko
