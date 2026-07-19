#pragma once

#include "gecko/core/assert.h"
#include "gecko/core/services/memory.h"
#include "gecko/core/types.h"
#include "gecko/core/utility/move.h"

namespace gecko {

class StringView
{
public:
  static constexpr usize NotFound = USizeMax;

  constexpr StringView() noexcept = default;
  constexpr StringView(const char* text) noexcept : m_Data(text), m_Count(LengthOf(text))
  {}
  constexpr StringView(const char* text, usize count) noexcept : m_Data(text), m_Count(count)
  {}

  [[nodiscard]] constexpr const char* Data() const noexcept
  {
    return m_Data;
  }
  [[nodiscard]] constexpr usize Count() const noexcept
  {
    return m_Count;
  }
  [[nodiscard]] constexpr usize Size() const noexcept
  {
    return m_Count;
  }
  [[nodiscard]] constexpr bool Empty() const noexcept
  {
    return m_Count == 0;
  }
  [[nodiscard]] constexpr const char* data() const noexcept
  {
    return m_Data;
  }
  [[nodiscard]] constexpr usize size() const noexcept
  {
    return m_Count;
  }
  [[nodiscard]] constexpr bool empty() const noexcept
  {
    return Empty();
  }
  [[nodiscard]] constexpr char Front() const noexcept
  {
    GECKO_ASSERT(m_Count != 0, "Cannot read the front of an empty StringView");
    return m_Data[0];
  }
  [[nodiscard]] constexpr char operator[](usize index) const noexcept
  {
    GECKO_ASSERT(index < m_Count, "StringView index is out of bounds");
    return m_Data[index];
  }

  [[nodiscard]] constexpr StringView Substring(usize offset, usize count = USizeMax) const noexcept
  {
    if (offset > m_Count)
      return {};
    const usize remaining = m_Count - offset;
    return StringView {m_Data + offset, count < remaining ? count : remaining};
  }

  [[nodiscard]] constexpr usize Find(char character, usize offset = 0) const noexcept
  {
    for (usize index = offset; index < m_Count; ++index)
      if (m_Data[index] == character)
        return index;
    return NotFound;
  }

  [[nodiscard]] constexpr usize FindLast(char character) const noexcept
  {
    for (usize index = m_Count; index != 0; --index)
      if (m_Data[index - 1U] == character)
        return index - 1U;
    return NotFound;
  }

  [[nodiscard]] constexpr bool StartsWith(StringView prefix) const noexcept
  {
    if (prefix.m_Count > m_Count)
      return false;
    for (usize index = 0; index < prefix.m_Count; ++index)
      if (m_Data[index] != prefix.m_Data[index])
        return false;
    return true;
  }

  friend constexpr bool operator==(StringView first, StringView second) noexcept
  {
    if (first.m_Count != second.m_Count)
      return false;
    for (usize index = 0; index < first.m_Count; ++index)
      if (first.m_Data[index] != second.m_Data[index])
        return false;
    return true;
  }

private:
  [[nodiscard]] static constexpr usize LengthOf(const char* text) noexcept
  {
    if (text == nullptr)
      return 0;
    usize count = 0;
    while (text[count] != '\0')
      ++count;
    return count;
  }

  const char* m_Data {nullptr};
  usize m_Count {0};
};

class String
{
public:
  String() noexcept = default;
  String(StringView text) noexcept
  {
    Assign(text);
  }
  String(const char* text) noexcept : String(StringView {text})
  {}
  String(const String& other) noexcept : String(other.View())
  {}
  String(String&& other) noexcept
      : m_Data(other.m_Data), m_Count(other.m_Count), m_Capacity(other.m_Capacity)
  {
    other.m_Data = nullptr;
    other.m_Count = 0;
    other.m_Capacity = 0;
  }
  ~String() noexcept
  {
    DeallocBytes(m_Data);
  }

  String& operator=(const String& other) noexcept
  {
    if (this != &other)
      Assign(other.View());
    return *this;
  }
  String& operator=(String&& other) noexcept
  {
    if (this != &other)
    {
      DeallocBytes(m_Data);
      m_Data = other.m_Data;
      m_Count = other.m_Count;
      m_Capacity = other.m_Capacity;
      other.m_Data = nullptr;
      other.m_Count = 0;
      other.m_Capacity = 0;
    }
    return *this;
  }
  String& operator=(StringView text) noexcept
  {
    Assign(text);
    return *this;
  }
  String& operator=(const char* text) noexcept
  {
    Assign(StringView {text});
    return *this;
  }

  void Reserve(usize capacity) noexcept
  {
    if (capacity <= m_Capacity)
      return;
    char* replacement = AllocArray<char>(capacity + 1U);
    for (usize index = 0; index < m_Count; ++index)
      replacement[index] = m_Data[index];
    replacement[m_Count] = '\0';
    DeallocBytes(m_Data);
    m_Data = replacement;
    m_Capacity = capacity;
  }

  void Resize(usize count, char value = '\0') noexcept
  {
    if (count > m_Capacity)
      Reserve(count > m_Capacity * 2U ? count : m_Capacity * 2U + 8U);
    for (usize index = m_Count; index < count; ++index)
      m_Data[index] = value;
    m_Count = count;
    if (m_Data != nullptr)
      m_Data[m_Count] = '\0';
  }

  void Clear() noexcept
  {
    m_Count = 0;
    if (m_Data != nullptr)
      m_Data[0] = '\0';
  }

  void Assign(StringView text) noexcept
  {
    Resize(text.Count());
    for (usize index = 0; index < text.Count(); ++index)
      m_Data[index] = text[index];
  }

  void Append(StringView text) noexcept
  {
    const usize offset = m_Count;
    Resize(m_Count + text.Count());
    for (usize index = 0; index < text.Count(); ++index)
      m_Data[offset + index] = text[index];
  }

  void Append(char character) noexcept
  {
    const usize offset = m_Count;
    Resize(m_Count + 1U);
    m_Data[offset] = character;
  }

  [[nodiscard]] const char* Data() const noexcept
  {
    return m_Data != nullptr ? m_Data : "";
  }
  [[nodiscard]] char* Data() noexcept
  {
    return m_Data;
  }
  [[nodiscard]] const char* CStr() const noexcept
  {
    return Data();
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
  [[nodiscard]] StringView View() const noexcept
  {
    return StringView {Data(), m_Count};
  }
  [[nodiscard]] operator StringView() const noexcept
  {
    return View();
  }
  [[nodiscard]] char& operator[](usize index) noexcept
  {
    GECKO_ASSERT(index < m_Count, "String index is out of bounds");
    return m_Data[index];
  }
  [[nodiscard]] const char& operator[](usize index) const noexcept
  {
    GECKO_ASSERT(index < m_Count, "String index is out of bounds");
    return m_Data[index];
  }

  [[nodiscard]] const char* c_str() const noexcept
  {
    return CStr();
  }
  [[nodiscard]] char* data() noexcept
  {
    return Data();
  }
  [[nodiscard]] const char* data() const noexcept
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
  void clear() noexcept
  {
    Clear();
  }
  void resize(usize count) noexcept
  {
    Resize(count);
  }
  void append(StringView text) noexcept
  {
    Append(text);
  }
  void push_back(char character) noexcept
  {
    Append(character);
  }
  void assign(StringView text) noexcept
  {
    Assign(text);
  }

private:
  char* m_Data {nullptr};
  usize m_Count {0};
  usize m_Capacity {0};
};

template <usize Capacity>
class StaticString
{
public:
  void Clear() noexcept
  {
    m_Count = 0;
    m_Data[0] = '\0';
  }
  void Append(StringView text) noexcept
  {
    GECKO_ASSERT(m_Count + text.Count() <= Capacity, "StaticString capacity exceeded");
    for (usize index = 0; index < text.Count(); ++index)
      m_Data[m_Count++] = text[index];
    m_Data[m_Count] = '\0';
  }
  void Append(char character) noexcept
  {
    GECKO_ASSERT(m_Count != Capacity, "StaticString capacity exceeded");
    m_Data[m_Count++] = character;
    m_Data[m_Count] = '\0';
  }
  [[nodiscard]] StringView View() const noexcept
  {
    return StringView {m_Data, m_Count};
  }
  [[nodiscard]] const char* Data() const noexcept
  {
    return m_Data;
  }
  [[nodiscard]] usize Count() const noexcept
  {
    return m_Count;
  }

private:
  char m_Data[Capacity + 1U] {};
  usize m_Count {0};
};

}  // namespace gecko
