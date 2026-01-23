#pragma once

#include "gecko/core/api.h"
#include "gecko/core/types.h"

namespace gecko {

GECKO_API u32 RandomU32(u32 min = 0, u32 max = UINT32_MAX) noexcept;
GECKO_API u64 RandomU64(u64 min = 0, u64 max = UINT64_MAX) noexcept;
GECKO_API i32 RandomI32(i32 min = INT32_MIN, i32 max = INT32_MAX) noexcept;
GECKO_API i64 RandomI64(i64 min = INT64_MIN, i64 max = INT64_MAX) noexcept;

GECKO_API f32 RandomF32(f32 min = 0.0f, f32 max = 1.0f) noexcept;
GECKO_API f64 RandomF64(f64 min = 0.0, f64 max = 1.0) noexcept;

GECKO_API bool RandomBool() noexcept;

GECKO_API void RandomBytes(void* buffer, usize size) noexcept;

GECKO_API void SeedRandom(u64 seed) noexcept;

namespace random {

inline usize Size(usize min, usize max) noexcept
{
  return static_cast<usize>(RandomU64(min, max));
}

inline usize Index(usize size) noexcept
{
  return size > 0 ? static_cast<usize>(RandomU64(0, size - 1)) : 0;
}

inline f32 Normalized() noexcept
{
  return RandomF32(0.0f, 1.0f);
}

inline f32 Signed() noexcept
{
  return RandomF32(-1.0f, 1.0f);
}

template <typename T>
const T& Choose(const T& a, const T& b) noexcept
{
  return RandomBool() ? a : b;
}

}  // namespace random

}  // namespace gecko
