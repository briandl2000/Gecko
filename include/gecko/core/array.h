#pragma once

/// @file
/// `gecko::Array<T>` -- move-only allocator-aware dynamic array.

#include "gecko/core/assert.h"
#include "gecko/core/services/memory.h"
#include "gecko/core/span.h"
#include "gecko/core/types.h"

#include <new>
#include <type_traits>
#include <utility>

namespace gecko {

template <class T>
class Array
{
public:
  Array() noexcept : m_Allocator(&CurrentAllocator())
  {}

  explicit Array(IAllocator& allocator) noexcept : m_Allocator(&allocator)
  {}

  Array(const Array&) = delete;
  Array& operator=(const Array&) = delete;

  Array(Array&& other) noexcept
      : m_Data(other.m_Data), m_Count(other.m_Count), m_Capacity(other.m_Capacity), m_Allocator(other.m_Allocator)
  {
    other.m_Data = nullptr;
    other.m_Count = 0;
    other.m_Capacity = 0;
    other.m_Allocator = &CurrentAllocator();
  }

  Array& operator=(Array&& other) noexcept
  {
    if (this != &other)
    {
      Reset();
      m_Data = other.m_Data;
      m_Count = other.m_Count;
      m_Capacity = other.m_Capacity;
      m_Allocator = other.m_Allocator;

      other.m_Data = nullptr;
      other.m_Count = 0;
      other.m_Capacity = 0;
      other.m_Allocator = &CurrentAllocator();
    }
    return *this;
  }

  ~Array() noexcept
  {
    Reset();
  }

  [[nodiscard]] T* Data() noexcept
  {
    return m_Data;
  }

  [[nodiscard]] const T* Data() const noexcept
  {
    return m_Data;
  }

  [[nodiscard]] usize Count() const noexcept
  {
    return m_Count;
  }

  [[nodiscard]] usize Capacity() const noexcept
  {
    return m_Capacity;
  }

  [[nodiscard]] bool Empty() const noexcept
  {
    return m_Count == 0;
  }

  [[nodiscard]] IAllocator* Allocator() const noexcept
  {
    return m_Allocator;
  }

  [[nodiscard]] Span<T> AsSpan() noexcept
  {
    return {m_Data, m_Count};
  }

  [[nodiscard]] Span<const T> AsSpan() const noexcept
  {
    return {m_Data, m_Count};
  }

  [[nodiscard]] T& operator[](usize index) noexcept
  {
    GECKO_ASSERT(index < m_Count && "Array index out of bounds");
    return m_Data[index];
  }

  [[nodiscard]] const T& operator[](usize index) const noexcept
  {
    GECKO_ASSERT(index < m_Count && "Array index out of bounds");
    return m_Data[index];
  }

  [[nodiscard]] T* begin() noexcept
  {
    return m_Data;
  }

  [[nodiscard]] T* end() noexcept
  {
    return m_Data == nullptr ? nullptr : m_Data + m_Count;
  }

  [[nodiscard]] const T* begin() const noexcept
  {
    return m_Data;
  }

  [[nodiscard]] const T* end() const noexcept
  {
    return m_Data == nullptr ? nullptr : m_Data + m_Count;
  }

  bool Reserve(usize capacity) noexcept
  {
    if (capacity <= m_Capacity)
      return true;
    return Reallocate(capacity);
  }

  template <class... Args>
  T* EmplaceBack(Args&&... args) noexcept
  {
    if (m_Count == m_Capacity)
    {
      const usize next = m_Capacity == 0 ? 8 : m_Capacity * 2;
      if (!Reserve(next))
        return nullptr;
    }

    T* slot = m_Data + m_Count;
    new (slot) T(::std::forward<Args>(args)...);
    ++m_Count;
    return slot;
  }

  bool PushBack(const T& value) noexcept
  {
    return EmplaceBack(value) != nullptr;
  }

  bool PushBack(T&& value) noexcept
  {
    return EmplaceBack(::std::move(value)) != nullptr;
  }

  void Clear() noexcept
  {
    DestroyRange(m_Data, m_Count);
    m_Count = 0;
  }

  void Reset() noexcept
  {
    Clear();
    if (m_Data != nullptr)
    {
      m_Allocator->Free(m_Data);
      m_Data = nullptr;
    }
    m_Capacity = 0;
  }

private:
  static void DestroyRange(T* data, usize count) noexcept
  {
    if constexpr (!::std::is_trivially_destructible_v<T>)
    {
      for (usize i = count; i > 0; --i)
        data[i - 1].~T();
    }
  }

  bool Reallocate(usize capacity) noexcept
  {
    if (capacity > (static_cast<usize>(-1) / sizeof(T)))
      return false;

    auto* newData = static_cast<T*>(m_Allocator->Alloc(sizeof(T) * capacity, alignof(T)));
    if (newData == nullptr)
      return false;

    for (usize i = 0; i < m_Count; ++i)
    {
      new (newData + i) T(::std::move(m_Data[i]));
    }

    DestroyRange(m_Data, m_Count);
    if (m_Data != nullptr)
      m_Allocator->Free(m_Data);

    m_Data = newData;
    m_Capacity = capacity;
    return true;
  }

  T* m_Data {nullptr};
  usize m_Count {0};
  usize m_Capacity {0};
  IAllocator* m_Allocator {nullptr};
};

}  // namespace gecko
