#pragma once

namespace gecko {

template <typename T>
struct RemoveReference
{
  using Type = T;
};

template <typename T>
struct RemoveReference<T&>
{
  using Type = T;
};

template <typename T>
struct RemoveReference<T&&>
{
  using Type = T;
};

template <typename T>
struct RemoveConst
{
  using Type = T;
};

template <typename T>
struct RemoveConst<const T>
{
  using Type = T;
};

template <typename T>
using RemoveCVRef = typename RemoveConst<typename RemoveReference<T>::Type>::Type;

template <bool Condition, typename TrueType, typename FalseType>
struct Conditional
{
  using Type = FalseType;
};

template <typename TrueType, typename FalseType>
struct Conditional<true, TrueType, FalseType>
{
  using Type = TrueType;
};

template <typename T>
[[nodiscard]] constexpr typename RemoveReference<T>::Type&& Move(T&& value) noexcept
{
  return static_cast<typename RemoveReference<T>::Type&&>(value);
}

template <typename T>
[[nodiscard]] constexpr T&& Forward(typename RemoveReference<T>::Type& value) noexcept
{
  return static_cast<T&&>(value);
}

template <typename T>
[[nodiscard]] constexpr T&& Forward(typename RemoveReference<T>::Type&& value) noexcept
{
  return static_cast<T&&>(value);
}

template <typename T>
constexpr void Swap(T& first, T& second) noexcept
{
  T temporary = Move(first);
  first = Move(second);
  second = Move(temporary);
}

}  // namespace gecko
