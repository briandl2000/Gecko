#pragma once

/// @file
/// `gecko::Span<T>` -- ABI-stable pointer/length view, intentionally
/// layout-compatible with `std::span` but with a fixed memory layout
/// we control.
///
/// Used at the `CoreServices` shared-library boundary in place of
/// `std::span<T>` so that virtual methods on Gecko service interfaces
/// (`IModule::Publishes()`, `IProfilerSink::WriteBatch(...)`, etc.)
/// don't depend on a particular standard-library implementation.
/// Inside a single translation unit you can still use `std::span` for
/// algorithms; convert at the boundary.

#include "gecko/core/types.h"

#include <type_traits>

namespace gecko {

/// Trivially-copyable pointer + length view.
///
/// Layout is `{ T* Data; usize Count; }`, identical on every supported
/// compiler. `T` must be a complete type at the use site; `Span<void>`
/// is intentionally unsupported (use `Span<const byte>`).
template <typename T>
class Span
{
public:
  /// Empty view.
  constexpr Span() noexcept = default;

  /// View over `count` elements starting at `data`.
  constexpr Span(T* data, usize count) noexcept : m_Data(data), m_Count(count)
  {}

  /// View over a C array.
  template <usize N>
  constexpr Span(T (&array)[N]) noexcept : m_Data(array), m_Count(N)
  {}

  /// Implicit conversion from `Span<T>` to `Span<const T>`.
  template <typename U,
            typename = ::std::enable_if_t<::std::is_same_v<::std::remove_const_t<T>, U>&& ::std::is_const_v<T>>>
  constexpr Span(const Span<U>& other) noexcept : m_Data(other.Data()), m_Count(other.Count())
  {}

  /// @returns Pointer to the first element, or `nullptr` if `Empty()`.
  [[nodiscard]] constexpr T* Data() const noexcept
  {
    return m_Data;
  }
  /// @returns Number of elements in the view.
  [[nodiscard]] constexpr usize Count() const noexcept
  {
    return m_Count;
  }
  /// @returns `true` when the view contains zero elements.
  [[nodiscard]] constexpr bool Empty() const noexcept
  {
    return m_Count == 0;
  }

  /// `std::span`-compatible alias for `Data()`.
  [[nodiscard]] constexpr T* data() const noexcept
  {
    return m_Data;
  }
  /// `std::span`-compatible alias for `Count()`.
  [[nodiscard]] constexpr usize size() const noexcept
  {
    return m_Count;
  }
  /// `std::span`-compatible alias for `Empty()`.
  [[nodiscard]] constexpr bool empty() const noexcept
  {
    return m_Count == 0;
  }

  /// @returns Iterator to the first element (range-for support).
  [[nodiscard]] constexpr T* begin() const noexcept
  {
    return m_Data;
  }
  /// @returns One-past-the-end iterator (range-for support).
  [[nodiscard]] constexpr T* end() const noexcept
  {
    return m_Count == 0 ? m_Data : m_Data + m_Count;
  }

  /// Unchecked element access.
  /// @param index Zero-based offset; must be < `Count()`.
  [[nodiscard]] constexpr T& operator[](usize index) const noexcept
  {
    return m_Data[index];
  }

private:
  T* m_Data {nullptr};
  usize m_Count {0};
};

static_assert(::std::is_trivially_copyable_v<Span<const int>>, "Span<T> must be trivially copyable to be ABI-stable.");

}  // namespace gecko
