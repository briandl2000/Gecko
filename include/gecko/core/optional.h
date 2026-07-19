#pragma once

#include "gecko/core/assert.h"
#include "gecko/core/placement.h"
#include "gecko/core/types.h"
#include "gecko/core/utility/move.h"

namespace gecko {

template <typename T>
class Optional
{
public:
  Optional() noexcept = default;
  Optional(const T& value) noexcept
  {
    new (&m_Storage.Value, Placement) T(value);
    m_HasValue = true;
  }
  Optional(T&& value) noexcept
  {
    new (&m_Storage.Value, Placement) T(Move(value));
    m_HasValue = true;
  }
  Optional(const Optional& other) noexcept
  {
    if (other.m_HasValue)
    {
      new (&m_Storage.Value, Placement) T(other.Value());
      m_HasValue = true;
    }
  }
  Optional(Optional&& other) noexcept
  {
    if (other.m_HasValue)
    {
      new (&m_Storage.Value, Placement) T(Move(other.Value()));
      m_HasValue = true;
      other.Reset();
    }
  }
  ~Optional() noexcept
  {
    Reset();
  }

  Optional& operator=(const Optional& other) noexcept
  {
    if (this == &other)
      return *this;
    Reset();
    if (other.m_HasValue)
    {
      new (&m_Storage.Value, Placement) T(other.Value());
      m_HasValue = true;
    }
    return *this;
  }
  Optional& operator=(Optional&& other) noexcept
  {
    if (this == &other)
      return *this;
    Reset();
    if (other.m_HasValue)
    {
      new (&m_Storage.Value, Placement) T(Move(other.Value()));
      m_HasValue = true;
      other.Reset();
    }
    return *this;
  }

  void Reset() noexcept
  {
    if (m_HasValue)
      Value().~T();
    m_HasValue = false;
  }
  [[nodiscard]] bool HasValue() const noexcept
  {
    return m_HasValue;
  }
  [[nodiscard]] explicit operator bool() const noexcept
  {
    return m_HasValue;
  }
  [[nodiscard]] T& Value() noexcept
  {
    GECKO_ASSERT(m_HasValue, "Optional has no value");
    return m_Storage.Value;
  }
  [[nodiscard]] const T& Value() const noexcept
  {
    GECKO_ASSERT(m_HasValue, "Optional has no value");
    return m_Storage.Value;
  }
  [[nodiscard]] T& operator*() noexcept
  {
    return Value();
  }
  [[nodiscard]] const T& operator*() const noexcept
  {
    return Value();
  }
  [[nodiscard]] T* operator->() noexcept
  {
    return &Value();
  }
  [[nodiscard]] const T* operator->() const noexcept
  {
    return &Value();
  }

private:
  union Storage
  {
    constexpr Storage() noexcept : Empty {}
    {}
    ~Storage() noexcept
    {}

    byte Empty;
    T Value;
  } m_Storage;
  bool m_HasValue {false};
};

}  // namespace gecko
