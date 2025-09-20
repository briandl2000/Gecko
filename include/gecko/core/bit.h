#pragma once

#include <type_traits>
#include "types.h"

namespace gecko {

  constexpr u64 bit(unsigned shift) noexcept {
    return (shift < 64) ? (u64{1} << shift) : 0ull;
  }

  template <class E>
  constexpr auto to_underlying(E e) noexcept {
    return static_cast<std::underlying_type_t<E>>(e);
  }

  template <class E>
  concept enum_flag = std::is_enum_v<E>;

  template <enum_flag E>
  constexpr E operator|(E a, E b) noexcept {
    using U = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<U>(a) | static_cast<U>(b));
  }
  template <enum_flag E>
  constexpr E operator&(E a, E b) noexcept {
    using U = std::underlying_type_t<E>;
    return static_cast<E>(static_cast<U>(a) & static_cast<U>(b));
  }
  template <enum_flag E>
  constexpr E& operator|=(E& a, E& b) noexcept { return a = a | b; }
  template <enum_flag E>
  constexpr bool any(E a) noexcept { return to_underlying(a) != 0; }
}
