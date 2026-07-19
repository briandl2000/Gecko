#pragma once

#include "gecko/core/types.h"

namespace gecko {

template <typename T>
class Span
{
public:
  constexpr Span() noexcept = default;
  constexpr Span(T* data, usize count) noexcept : m_Data(data), m_Count(count)
  {}

  template <usize Count>
  constexpr Span(T (&array)[Count]) noexcept : m_Data(array), m_Count(Count)
  {}

  template <typename U>
    requires requires(U* value) { static_cast<T*>(value); }
  constexpr Span(const Span<U>& other) noexcept : m_Data(other.Data()), m_Count(other.Count())
  {}

  [[nodiscard]] constexpr T* Data() const noexcept
  {
    return m_Data;
  }

  [[nodiscard]] constexpr usize Count() const noexcept
  {
    return m_Count;
  }

  [[nodiscard]] constexpr bool Empty() const noexcept
  {
    return m_Count == 0;
  }

  [[nodiscard]] constexpr T* data() const noexcept
  {
    return m_Data;
  }

  [[nodiscard]] constexpr usize size() const noexcept
  {
    return m_Count;
  }

  [[nodiscard]] constexpr bool empty() const noexcept
  {
    return m_Count == 0;
  }

  [[nodiscard]] constexpr T* begin() const noexcept
  {
    return m_Data;
  }

  [[nodiscard]] constexpr T* end() const noexcept
  {
    return m_Data + m_Count;
  }

  [[nodiscard]] constexpr T& operator[](usize index) const noexcept
  {
    return m_Data[index];
  }

private:
  T* m_Data {nullptr};
  usize m_Count {0};
};

static_assert(__is_trivially_copyable(Span<const int>));

}  // namespace gecko
