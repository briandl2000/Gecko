#pragma once

/// @file
/// `gecko::StringView` -- ABI-stable borrowed UTF-8/string byte view.
///
/// `StringView` never owns memory. The referenced bytes must outlive the
/// view. The data does not need to be null-terminated.

#include "gecko/core/types.h"

#include <type_traits>

namespace gecko {

class StringView
{
public:
  constexpr StringView() noexcept = default;

  constexpr StringView(const char* data, usize size) noexcept : m_Data(data), m_Size(size)
  {}

  template <usize N>
  constexpr StringView(const char (&literal)[N]) noexcept : m_Data(literal), m_Size(N > 0 ? N - 1 : 0)
  {}

  [[nodiscard]] constexpr const char* Data() const noexcept
  {
    return m_Data;
  }

  [[nodiscard]] constexpr usize Size() const noexcept
  {
    return m_Size;
  }

  [[nodiscard]] constexpr bool Empty() const noexcept
  {
    return m_Size == 0;
  }

  [[nodiscard]] constexpr const char* data() const noexcept
  {
    return m_Data;
  }

  [[nodiscard]] constexpr usize size() const noexcept
  {
    return m_Size;
  }

  [[nodiscard]] constexpr bool empty() const noexcept
  {
    return Empty();
  }

  [[nodiscard]] constexpr const char* begin() const noexcept
  {
    return m_Data;
  }

  [[nodiscard]] constexpr const char* end() const noexcept
  {
    return m_Size == 0 ? m_Data : m_Data + m_Size;
  }

  [[nodiscard]] constexpr char operator[](usize index) const noexcept
  {
    return m_Data[index];
  }

private:
  const char* m_Data {nullptr};
  usize m_Size {0};
};

[[nodiscard]] constexpr bool operator==(StringView a, StringView b) noexcept
{
  if (a.Size() != b.Size())
    return false;
  for (usize i = 0; i < a.Size(); ++i)
  {
    if (a[i] != b[i])
      return false;
  }
  return true;
}

[[nodiscard]] constexpr bool operator!=(StringView a, StringView b) noexcept
{
  return !(a == b);
}

static_assert(::std::is_trivially_copyable_v<StringView>, "StringView must be trivially copyable.");

}  // namespace gecko
