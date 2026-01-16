#include "gecko/core/time.h"

#include <chrono>

#include "gecko/core/profiler.h"
#include "labels.h"
namespace gecko {

u64 MonotonicTimeNs() noexcept {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

u64 HighResTimeNs() noexcept {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::high_resolution_clock::now().time_since_epoch())
      .count();
}

u64 SystemTimeNs() noexcept {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

} // namespace gecko
