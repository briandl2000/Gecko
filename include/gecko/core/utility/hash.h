#pragma once

#include "gecko/core/types.h"

namespace gecko {

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
