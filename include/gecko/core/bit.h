#pragma once

// System includes first (alphabetical order)
#include <type_traits>

// Project includes second (alphabetical order)
#include "gecko/core/api.h"
#include "gecko/core/types.h"

namespace gecko {

  GECKO_API constexpr u64 Bit(unsigned shift) noexcept 
  {
    return (shift < 64) ? (u64{1} << shift) : 0ull;
  }

  template <class E>
  GECKO_API constexpr auto ToUnderlying(E e) noexcept 
  {
    return static_cast<std::underlying_type_t<E>>(e);
  }

  template <class E>
  concept EnumFlag = std::is_enum_v<E>;

  template <EnumFlag E>
  GECKO_API constexpr E operator|(E a, E b) noexcept 
  {
    using U = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<U>(a) | static_cast<U>(b));
  }
  
  template <EnumFlag E>
  GECKO_API constexpr E operator&(E a, E b) noexcept {
    using U = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<U>(a) & static_cast<U>(b));
  }

  template <EnumFlag E>
  GECKO_API constexpr E& operator|=(E& a, E& b) noexcept { return a = a | b; }

  template <EnumFlag E>
  GECKO_API constexpr bool Any(E a) noexcept { return ToUnderlying(a) != 0; }
}
