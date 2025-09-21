#pragma once
#include "types.h"

namespace gecko {

  struct Category
  {
    u32 id { 0 };
    const char* name { nullptr };
    constexpr explicit operator u32() const noexcept { return id; }
  };

  constexpr u32 FNV1a(const char* s) 
  {
    u32 h = 2166136261u; 
    while(*s) { h^=(u8)*s++; h*=16777619u; }
    return h; 
  }

  constexpr Category MakeCategory(const char* name) 
  {
    return Category { FNV1a(name), name };
  }
}
