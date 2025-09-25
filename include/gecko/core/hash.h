#pragma once

#include "types.h"

namespace gecko {

  constexpr u32 FNV1a(const char* s) noexcept
  {
    u32 h = 2166136261u; 
    while(*s) { h^=static_cast<u8>(*s++); h*=16777619u; }
    return h; 
  }

 constexpr u32 FNV1a(const void* data, std::size_t size) noexcept
  {
    const u8* bytes = static_cast<const u8*>(data);
    u32 h = 2166136261u;
    for (std::size_t i = 0; i < size; ++i)
    {
      h ^= bytes[i];
      h *= 16777619u;
    }
    return h;
  }

}
