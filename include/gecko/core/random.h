#pragma once

#include <cstdint>

#include "api.h"
#include "types.h"

namespace gecko {

// Thread-safe global random number generator
// Uses thread_local storage to avoid contention between threads

// Generate random integers in range [min, max] (inclusive)
GECKO_API u32 RandomU32(u32 min = 0, u32 max = UINT32_MAX) noexcept;
GECKO_API u64 RandomU64(u64 min = 0, u64 max = UINT64_MAX) noexcept;
GECKO_API i32 RandomI32(i32 min = INT32_MIN, i32 max = INT32_MAX) noexcept;
GECKO_API i64 RandomI64(i64 min = INT64_MIN, i64 max = INT64_MAX) noexcept;

// Generate random floating point numbers in range [min, max)
GECKO_API float RandomFloat(float min = 0.0f, float max = 1.0f) noexcept;
GECKO_API double RandomDouble(double min = 0.0, double max = 1.0) noexcept;

// Generate random boolean (50/50 chance)
GECKO_API bool RandomBool() noexcept;

// Generate random bytes
GECKO_API void RandomBytes(void *buffer, std::size_t size) noexcept;

// Seed the thread-local random generator with a specific value
// If not called, the generator will be seeded automatically with a
// random_device
GECKO_API void SeedRandom(u64 seed) noexcept;

// Utility functions for common patterns
namespace random {

// Generate random std::size_t in range [min, max] (inclusive)
inline std::size_t Size(std::size_t min, std::size_t max) noexcept {
  return static_cast<std::size_t>(RandomU64(min, max));
}

// Generate random index for array/vector of given size [0, size-1]
inline std::size_t Index(std::size_t size) noexcept {
  return size > 0 ? static_cast<std::size_t>(RandomU64(0, size - 1)) : 0;
}

// Generate random normalized float in range [0, 1)
inline float Normalized() noexcept { return RandomFloat(0.0f, 1.0f); }

// Generate random float in range [-1, 1)
inline float Signed() noexcept { return RandomFloat(-1.0f, 1.0f); }

// Randomly choose between two values (50/50 chance)
template <typename T> const T &Choose(const T &a, const T &b) noexcept {
  return RandomBool() ? a : b;
}

} // namespace random

} // namespace gecko
