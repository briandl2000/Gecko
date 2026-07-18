#pragma once

#include "gecko/core/assert.h"
#include "gecko/core/services/memory.h"
#include "gecko/core/types.h"
#include "gecko/core/utility/move.h"

#include <new>

namespace gecko {

template <typename T>
class Array
{
public:
  Array() noexcept = default;
  explicit Array(usize count) noexcept
  {
    if (count == 0)
      return;
    m_Data = AllocArray<T>(count);
    m_Capacity = count;
    while (m_Count < count)
      new (m_Data + m_Count++) T {};
  }
  Array(usize count, const T& value) noexcept
  {
    if (count == 0)
      return;
    m_Data = AllocArray<T>(count);
    m_Capacity = count;
    while (m_Count < count)
      new (m_Data + m_Count++) T(value);
  }
  Array(const Array& other) noexcept
  {
    Reserve(other.m_Count);
    for (const T& value : other)
      EmplaceBack(value);
  }
  Array(Array&& other) noexcept : m_Data(other.m_Data), m_Count(other.m_Count), m_Capacity(other.m_Capacity)
  {
    other.m_Data = nullptr;
    other.m_Count = 0;
    other.m_Capacity = 0;
  }
  ~Array() noexcept
  {
    Clear();
    DeallocBytes(m_Data);
  }

  Array& operator=(const Array& other) noexcept
  {
    if (this == &other)
      return *this;
    Clear();
    Reserve(other.m_Count);
    for (const T& value : other)
      EmplaceBack(value);
    return *this;
  }
  Array& operator=(Array&& other) noexcept
  {
    if (this == &other)
      return *this;
    Clear();
    DeallocBytes(m_Data);
    m_Data = other.m_Data;
    m_Count = other.m_Count;
    m_Capacity = other.m_Capacity;
    other.m_Data = nullptr;
    other.m_Count = 0;
    other.m_Capacity = 0;
    return *this;
  }

  void Reserve(usize capacity) noexcept
  {
    if (capacity <= m_Capacity)
      return;
    T* replacement = AllocArray<T>(capacity);
    for (usize index = 0; index < m_Count; ++index)
    {
      new (replacement + index) T(Move(m_Data[index]));
      m_Data[index].~T();
    }
    DeallocBytes(m_Data);
    m_Data = replacement;
    m_Capacity = capacity;
  }

  void Resize(usize count) noexcept
  {
    if (count > m_Capacity)
      Reserve(count > m_Capacity * 2U ? count : m_Capacity * 2U + 8U);
    while (m_Count < count)
      new (m_Data + m_Count++) T {};
    while (m_Count > count)
      m_Data[--m_Count].~T();
  }

  void Clear() noexcept
  {
    while (m_Count != 0)
      m_Data[--m_Count].~T();
  }

  void Assign(usize count, const T& value) noexcept
  {
    Clear();
    if (count > m_Capacity)
      Reserve(count);
    while (m_Count < count)
      new (m_Data + m_Count++) T(value);
  }

  template <typename... Args>
  T& EmplaceBack(Args&&... arguments) noexcept
  {
    if (m_Count == m_Capacity)
      Reserve(m_Capacity * 2U + 8U);
    T* value = new (m_Data + m_Count++) T(Forward<Args>(arguments)...);
    return *value;
  }

  void PushBack(const T& value) noexcept
  {
    EmplaceBack(value);
  }
  void PushBack(T&& value) noexcept
  {
    EmplaceBack(Move(value));
  }

  void PopBack() noexcept
  {
    GECKO_ASSERT(m_Count != 0, "Cannot pop an empty Array");
    m_Data[--m_Count].~T();
  }

  T* Erase(T* position) noexcept
  {
    GECKO_ASSERT(position >= m_Data && position < m_Data + m_Count, "Array erase iterator is out of bounds");
    const usize index = static_cast<usize>(position - m_Data);
    m_Data[index].~T();
    for (usize cursor = index; cursor + 1U < m_Count; ++cursor)
    {
      new (m_Data + cursor) T(Move(m_Data[cursor + 1U]));
      m_Data[cursor + 1U].~T();
    }
    --m_Count;
    return m_Data + index;
  }

  void Swap(Array& other) noexcept
  {
    gecko::Swap(m_Data, other.m_Data);
    gecko::Swap(m_Count, other.m_Count);
    gecko::Swap(m_Capacity, other.m_Capacity);
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
  [[nodiscard]] usize Size() const noexcept
  {
    return m_Count;
  }
  [[nodiscard]] bool Empty() const noexcept
  {
    return m_Count == 0;
  }
  [[nodiscard]] T& Front() noexcept
  {
    GECKO_ASSERT(m_Count != 0, "Cannot read the front of an empty Array");
    return m_Data[0];
  }
  [[nodiscard]] T& Back() noexcept
  {
    GECKO_ASSERT(m_Count != 0, "Cannot read the back of an empty Array");
    return m_Data[m_Count - 1U];
  }
  [[nodiscard]] const T& Back() const noexcept
  {
    GECKO_ASSERT(m_Count != 0, "Cannot read the back of an empty Array");
    return m_Data[m_Count - 1U];
  }
  [[nodiscard]] T& operator[](usize index) noexcept
  {
    GECKO_ASSERT(index < m_Count, "Array index is out of bounds");
    return m_Data[index];
  }
  [[nodiscard]] const T& operator[](usize index) const noexcept
  {
    GECKO_ASSERT(index < m_Count, "Array index is out of bounds");
    return m_Data[index];
  }

  [[nodiscard]] T* begin() noexcept
  {
    return m_Data;
  }
  [[nodiscard]] const T* begin() const noexcept
  {
    return m_Data;
  }
  [[nodiscard]] T* end() noexcept
  {
    return m_Data + m_Count;
  }
  [[nodiscard]] const T* end() const noexcept
  {
    return m_Data + m_Count;
  }

  [[nodiscard]] T* data() noexcept
  {
    return Data();
  }
  [[nodiscard]] const T* data() const noexcept
  {
    return Data();
  }
  [[nodiscard]] usize size() const noexcept
  {
    return Size();
  }
  [[nodiscard]] bool empty() const noexcept
  {
    return Empty();
  }
  void reserve(usize capacity) noexcept
  {
    Reserve(capacity);
  }
  void resize(usize count) noexcept
  {
    Resize(count);
  }
  void clear() noexcept
  {
    Clear();
  }
  void assign(usize count, const T& value) noexcept
  {
    Assign(count, value);
  }
  void push_back(const T& value) noexcept
  {
    PushBack(value);
  }
  void push_back(T&& value) noexcept
  {
    PushBack(Move(value));
  }
  template <typename... Args>
  T& emplace_back(Args&&... arguments) noexcept
  {
    return EmplaceBack(Forward<Args>(arguments)...);
  }
  void pop_back() noexcept
  {
    PopBack();
  }
  T* erase(T* position) noexcept
  {
    return Erase(position);
  }
  T& back() noexcept
  {
    return Back();
  }
  const T& back() const noexcept
  {
    return Back();
  }
  T& front() noexcept
  {
    return Front();
  }
  void swap(Array& other) noexcept
  {
    Swap(other);
  }

private:
  T* m_Data {nullptr};
  usize m_Count {0};
  usize m_Capacity {0};
};

}  // namespace gecko
