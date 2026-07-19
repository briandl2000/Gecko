#pragma once

#include "gecko/core/types.h"

namespace gecko {

inline void MemoryCopy(void* destination, const void* source, usize size) noexcept
{
  auto* output = static_cast<u8*>(destination);
  const auto* input = static_cast<const u8*>(source);
  for (usize index = 0; index < size; ++index)
    output[index] = input[index];
}

inline void MemoryMove(void* destination, const void* source, usize size) noexcept
{
  auto* output = static_cast<u8*>(destination);
  const auto* input = static_cast<const u8*>(source);
  if (output < input)
  {
    for (usize index = 0; index < size; ++index)
      output[index] = input[index];
  }
  else if (output > input)
  {
    for (usize index = size; index != 0; --index)
      output[index - 1U] = input[index - 1U];
  }
}

inline void MemorySet(void* destination, u8 value, usize size) noexcept
{
  auto* output = static_cast<u8*>(destination);
  for (usize index = 0; index < size; ++index)
    output[index] = value;
}

inline int StringCompare(const char* first, const char* second) noexcept
{
  while (*first != '\0' && *first == *second)
  {
    ++first;
    ++second;
  }
  return static_cast<unsigned char>(*first) - static_cast<unsigned char>(*second);
}

inline void StringCopy(char* destination, usize capacity, const char* source) noexcept
{
  if (capacity == 0)
    return;
  usize index = 0;
  while (source != nullptr && source[index] != '\0' && index + 1U < capacity)
  {
    destination[index] = source[index];
    ++index;
  }
  destination[index] = '\0';
}

}  // namespace gecko
