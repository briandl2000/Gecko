#pragma once

#include "gecko/api.h"
#include "gecko/core/types.h"

namespace gecko {

GECKO_API constexpr u64 Bit(unsigned shift) noexcept
{
  return (shift < 64) ? (u64 {1} << shift) : 0ull;
}

template <class E>
GECKO_API constexpr auto ToUnderlying(E e) noexcept
{
  return static_cast<__underlying_type(E)>(e);
}

template <class E>
concept EnumFlag = __is_enum(E);

template <EnumFlag E>
GECKO_API constexpr E operator|(E a, E b) noexcept
{
  using U = __underlying_type(E);
  return static_cast<E>(static_cast<U>(a) | static_cast<U>(b));
}

template <EnumFlag E>
GECKO_API constexpr E operator&(E a, E b) noexcept
{
  using U = __underlying_type(E);
  return static_cast<E>(static_cast<U>(a) & static_cast<U>(b));
}

template <EnumFlag E>
GECKO_API constexpr E& operator|=(E& a, E b) noexcept
{
  return a = a | b;
}

template <EnumFlag E>
GECKO_API constexpr E operator^(E a, E b) noexcept
{
  using U = __underlying_type(E);
  return static_cast<E>(static_cast<U>(a) ^ static_cast<U>(b));
}

template <EnumFlag E>
GECKO_API constexpr E& operator^=(E& a, E b) noexcept
{
  return a = a ^ b;
}

template <EnumFlag E>
GECKO_API constexpr bool Any(E a) noexcept
{
  return ToUnderlying(a) != 0;
}

template <EnumFlag E>
GECKO_API constexpr E operator~(E a) noexcept
{
  using U = __underlying_type(E);
  return static_cast<E>(~static_cast<U>(a));
}

template <EnumFlag E>
GECKO_API constexpr E& operator&=(E& a, E b) noexcept
{
  return a = a & b;
}
}  // namespace gecko
