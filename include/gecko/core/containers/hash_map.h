#pragma once

#include "gecko/core/assert.h"
#include "gecko/core/types.h"
#include "gecko/core/utility/move.h"

namespace gecko {

template <typename First, typename Second>
struct Pair
{
  First first;
  Second second;
};

template <typename Key, typename Value, usize Capacity = 128>
class HashMap
{
  struct Slot
  {
    Pair<Key, Value> Entry {};
    bool Active {false};
  };

public:
  template <bool Constant>
  class IteratorBase
  {
    using Map = typename Conditional<Constant, const HashMap, HashMap>::Type;
    using Entry = typename Conditional<Constant, const Pair<Key, Value>, Pair<Key, Value>>::Type;

  public:
    IteratorBase() noexcept = default;
    IteratorBase(Map* map, usize index) noexcept : m_Map(map), m_Index(index)
    {
      SkipEmpty();
    }
    Entry& operator*() const noexcept
    {
      return m_Map->m_Slots[m_Index].Entry;
    }
    Entry* operator->() const noexcept
    {
      return &m_Map->m_Slots[m_Index].Entry;
    }
    IteratorBase& operator++() noexcept
    {
      ++m_Index;
      SkipEmpty();
      return *this;
    }
    friend bool operator==(IteratorBase first, IteratorBase second) noexcept
    {
      return first.m_Map == second.m_Map && first.m_Index == second.m_Index;
    }
    friend bool operator!=(IteratorBase first, IteratorBase second) noexcept
    {
      return !(first == second);
    }

  private:
    friend class HashMap;
    void SkipEmpty() noexcept
    {
      while (m_Map != nullptr && m_Index < Capacity && !m_Map->m_Slots[m_Index].Active)
        ++m_Index;
    }
    Map* m_Map {nullptr};
    usize m_Index {Capacity};
  };

  using Iterator = IteratorBase<false>;
  using ConstIterator = IteratorBase<true>;

  [[nodiscard]] Iterator Find(const Key& key) noexcept
  {
    for (usize index = 0; index < Capacity; ++index)
      if (m_Slots[index].Active && m_Slots[index].Entry.first == key)
        return Iterator {this, index};
    return End();
  }
  [[nodiscard]] ConstIterator Find(const Key& key) const noexcept
  {
    for (usize index = 0; index < Capacity; ++index)
      if (m_Slots[index].Active && m_Slots[index].Entry.first == key)
        return ConstIterator {this, index};
    return End();
  }
  template <typename K, typename V>
  Pair<Iterator, bool> Emplace(K&& key, V&& value) noexcept
  {
    Iterator existing = Find(key);
    if (existing != End())
      return {existing, false};
    for (usize index = 0; index < Capacity; ++index)
    {
      if (m_Slots[index].Active)
        continue;
      m_Slots[index].Entry.first = Forward<K>(key);
      m_Slots[index].Entry.second = Forward<V>(value);
      m_Slots[index].Active = true;
      ++m_Count;
      return {Iterator {this, index}, true};
    }
    GECKO_ASSERT(false, "HashMap fixed capacity exceeded");
    return {End(), false};
  }
  Value& GetOrAdd(const Key& key) noexcept
  {
    Iterator existing = Find(key);
    if (existing != End())
      return existing->second;
    return Emplace(key, Value {}).first->second;
  }
  bool Erase(const Key& key) noexcept
  {
    Iterator position = Find(key);
    if (position == End())
      return false;
    (void)Erase(position);
    return true;
  }
  Iterator Erase(Iterator position) noexcept
  {
    if (position == End())
      return End();
    const usize index = position.m_Index;
    m_Slots[index].Entry = {};
    m_Slots[index].Active = false;
    --m_Count;
    return Iterator {this, index + 1U};
  }
  void Clear() noexcept
  {
    for (Slot& slot : m_Slots)
      slot = {};
    m_Count = 0;
  }

  Iterator Begin() noexcept
  {
    return Iterator {this, 0};
  }
  ConstIterator Begin() const noexcept
  {
    return ConstIterator {this, 0};
  }
  Iterator End() noexcept
  {
    return Iterator {this, Capacity};
  }
  ConstIterator End() const noexcept
  {
    return ConstIterator {this, Capacity};
  }
  usize Count() const noexcept
  {
    return m_Count;
  }
  bool Empty() const noexcept
  {
    return m_Count == 0;
  }
  Value& operator[](const Key& key) noexcept
  {
    return GetOrAdd(key);
  }

  Iterator find(const Key& key) noexcept { return Find(key); }
  ConstIterator find(const Key& key) const noexcept { return Find(key); }
  template <typename K, typename V>
  Pair<Iterator, bool> emplace(K&& key, V&& value) noexcept { return Emplace(Forward<K>(key), Forward<V>(value)); }
  bool erase(const Key& key) noexcept { return Erase(key); }
  Iterator erase(Iterator position) noexcept { return Erase(position); }
  void clear() noexcept { Clear(); }
  Iterator begin() noexcept { return Begin(); }
  ConstIterator begin() const noexcept { return Begin(); }
  Iterator end() noexcept { return End(); }
  ConstIterator end() const noexcept { return End(); }
  usize size() const noexcept { return Count(); }
  bool empty() const noexcept { return Empty(); }

private:
  Slot m_Slots[Capacity] {};
  usize m_Count {0};
};

}  // namespace gecko
