#pragma once

/// @file
/// Compile-time and runtime FNV-1a string/buffer hashes.
///
/// Used throughout the engine to derive stable u32/u64 ids from
/// names (`Label` strings, profiler scope names, service ids).

#include "gecko/core/types.h"

namespace gecko {

/// 32-bit FNV-1a of a NUL-terminated C string.
constexpr u32 FNV1a(const char* s) noexcept
{
  u32 h = 2166136261u;
  while (*s)
  {
    h ^= static_cast<u8>(*s++);
    h *= 16777619u;
  }
  return h;
}

// Force compile-time evaluation for string literals (C++20 consteval)
/// 32-bit FNV-1a forced to evaluate at compile time. Use for string
/// literals embedded in macros so the hash never costs runtime cycles.
consteval u32 FNV1aLiteral(const char* s) noexcept
{
  u32 h = 2166136261u;
  while (*s)
  {
    h ^= static_cast<u8>(*s++);
    h *= 16777619u;
  }
  return h;
}

/// 32-bit FNV-1a over a raw byte range.
constexpr u32 FNV1a(const void* data, usize size) noexcept
{
  const u8* bytes = static_cast<const u8*>(data);
  u32 h = 2166136261u;
  for (usize i = 0; i < size; ++i)
  {
    h ^= bytes[i];
    h *= 16777619u;
  }
  return h;
}

// 64-bit FNV-1a (for more precision).
/// 64-bit FNV-1a of a NUL-terminated C string.
constexpr u64 FNV1a64(const char* s) noexcept
{
  u64 h = 14695981039346656037ull;
  while (*s)
  {
    h ^= static_cast<u8>(*s++);
    h *= 1099511628211ull;
  }
  return h;
}

/// 64-bit FNV-1a over a raw byte range.
constexpr u64 FNV1a64(const void* data, usize size) noexcept
{
  const u8* bytes = static_cast<const u8*>(data);
  u64 h = 14695981039346656037ull;
  for (usize i = 0; i < size; ++i)
  {
    h ^= bytes[i];
    h *= 1099511628211ull;
  }
  return h;
}

}  // namespace gecko
