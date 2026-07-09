#pragma once

/// @file
/// `gecko::String` -- move-only allocator-aware UTF-8/string byte owner.

#include "gecko/core/services/memory.h"
#include "gecko/core/string_view.h"
#include "gecko/core/types.h"

#include <cstring>
#include <utility>

namespace gecko {

class String
{
public:
  String() noexcept : m_Allocator(&CurrentAllocator())
  {}

  explicit String(IAllocator& allocator) noexcept : m_Allocator(&allocator)
  {}

  explicit String(StringView view) noexcept : String()
  {
    (void)Assign(view);
  }

  String(IAllocator& allocator, StringView view) noexcept : String(allocator)
  {
    (void)Assign(view);
  }

  String(const String&) = delete;
  String& operator=(const String&) = delete;

  String(String&& other) noexcept
      : m_Data(other.m_Data), m_Size(other.m_Size), m_Capacity(other.m_Capacity), m_Allocator(other.m_Allocator)
  {
    other.m_Data = nullptr;
    other.m_Size = 0;
    other.m_Capacity = 0;
    other.m_Allocator = &CurrentAllocator();
  }

  String& operator=(String&& other) noexcept
  {
    if (this != &other)
    {
      Reset();
      m_Data = other.m_Data;
      m_Size = other.m_Size;
      m_Capacity = other.m_Capacity;
      m_Allocator = other.m_Allocator;

      other.m_Data = nullptr;
      other.m_Size = 0;
      other.m_Capacity = 0;
      other.m_Allocator = &CurrentAllocator();
    }
    return *this;
  }

  ~String() noexcept
  {
    Reset();
  }

  [[nodiscard]] const char* Data() const noexcept
  {
    return m_Data;
  }

  [[nodiscard]] char* Data() noexcept
  {
    return m_Data;
  }

  [[nodiscard]] const char* CStr() const noexcept
  {
    return m_Data == nullptr ? "" : m_Data;
  }

  [[nodiscard]] usize Size() const noexcept
  {
    return m_Size;
  }

  [[nodiscard]] usize Capacity() const noexcept
  {
    return m_Capacity;
  }

  [[nodiscard]] bool Empty() const noexcept
  {
    return m_Size == 0;
  }

  [[nodiscard]] IAllocator* Allocator() const noexcept
  {
    return m_Allocator;
  }

  [[nodiscard]] StringView View() const noexcept
  {
    return {m_Data, m_Size};
  }

  [[nodiscard]] operator StringView() const noexcept
  {
    return View();
  }

  bool Reserve(usize capacity) noexcept
  {
    if (capacity <= m_Capacity)
      return true;
    return Reallocate(capacity);
  }

  bool Assign(StringView text) noexcept
  {
    Clear();
    return Append(text);
  }

  bool Append(StringView text) noexcept
  {
    if (text.Empty())
      return true;
    if (!Reserve(m_Size + text.Size()))
      return false;
    ::std::memcpy(m_Data + m_Size, text.Data(), text.Size());
    m_Size += text.Size();
    m_Data[m_Size] = '\0';
    return true;
  }

  bool PushBack(char c) noexcept
  {
    if (!Reserve(m_Size + 1))
      return false;
    m_Data[m_Size] = c;
    ++m_Size;
    m_Data[m_Size] = '\0';
    return true;
  }

  void Clear() noexcept
  {
    m_Size = 0;
    if (m_Data != nullptr)
      m_Data[0] = '\0';
  }

  void Reset() noexcept
  {
    if (m_Data != nullptr)
    {
      m_Allocator->Free(m_Data);
      m_Data = nullptr;
    }
    m_Size = 0;
    m_Capacity = 0;
  }

private:
  bool Reallocate(usize capacity) noexcept
  {
    if (capacity == static_cast<usize>(-1))
      return false;

    auto* newData = static_cast<char*>(m_Allocator->Alloc(capacity + 1, alignof(char)));
    if (newData == nullptr)
      return false;

    if (m_Data != nullptr && m_Size > 0)
      ::std::memcpy(newData, m_Data, m_Size);
    newData[m_Size] = '\0';

    if (m_Data != nullptr)
      m_Allocator->Free(m_Data);

    m_Data = newData;
    m_Capacity = capacity;
    return true;
  }

  char* m_Data {nullptr};
  usize m_Size {0};
  usize m_Capacity {0};
  IAllocator* m_Allocator {nullptr};
};

}  // namespace gecko
