#pragma once

#include "gecko/core/placement.h"
#include "gecko/core/services/memory.h"

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace gecko {

namespace detail {

template <typename T>
void DestroyObject(void* object) noexcept
{
  if (object == nullptr)
    return;
  static_cast<T*>(object)->~T();
  DeallocBytes(object);
}

struct SharedControl
{
  u32 References {1};
  void* Object {nullptr};
  void (*Destroy)(SharedControl*) noexcept {nullptr};
};

inline void AddReference(SharedControl* control) noexcept
{
#if defined(_MSC_VER)
  (void)_InterlockedIncrement(reinterpret_cast<volatile long*>(&control->References));
#else
  (void)__atomic_add_fetch(&control->References, 1U, __ATOMIC_RELAXED);
#endif
}

inline bool RemoveReference(SharedControl* control) noexcept
{
#if defined(_MSC_VER)
  return _InterlockedDecrement(reinterpret_cast<volatile long*>(&control->References)) == 0;
#else
  return __atomic_sub_fetch(&control->References, 1U, __ATOMIC_ACQ_REL) == 0;
#endif
}

template <typename Deleter>
struct SharedControlWithDeleter final : SharedControl
{
  explicit SharedControlWithDeleter(void* object, Deleter&& deleter) noexcept
      : DeleterFunction(static_cast<Deleter&&>(deleter))
  {
    Object = object;
    Destroy = [](SharedControl* base) noexcept {
      auto* control = static_cast<SharedControlWithDeleter*>(base);
      control->DeleterFunction(control->Object);
      control->~SharedControlWithDeleter();
      DeallocBytes(control);
    };
  }

  Deleter DeleterFunction;
};

}  // namespace detail

template <typename T>
class Unique
{
  template <typename>
  friend class Unique;

public:
  constexpr Unique() noexcept = default;
  constexpr Unique(decltype(nullptr)) noexcept
  {}

  Unique(T* pointer, void (*destroy)(void*) noexcept) noexcept : m_Pointer(pointer), m_Destroy(destroy)
  {}

  ~Unique() noexcept
  {
    reset();
  }

  Unique(const Unique&) = delete;
  Unique& operator=(const Unique&) = delete;

  Unique(Unique&& other) noexcept : m_Pointer(other.m_Pointer), m_Destroy(other.m_Destroy)
  {
    other.m_Pointer = nullptr;
    other.m_Destroy = nullptr;
  }

  template <typename U>
    requires requires(U* value) { static_cast<T*>(value); }
  Unique(Unique<U>&& other) noexcept : m_Pointer(other.m_Pointer), m_Destroy(other.m_Destroy)
  {
    other.m_Pointer = nullptr;
    other.m_Destroy = nullptr;
  }

  Unique& operator=(Unique&& other) noexcept
  {
    if (this != &other)
    {
      reset();
      m_Pointer = other.m_Pointer;
      m_Destroy = other.m_Destroy;
      other.m_Pointer = nullptr;
      other.m_Destroy = nullptr;
    }
    return *this;
  }

  template <typename U>
    requires requires(U* value) { static_cast<T*>(value); }
  Unique& operator=(Unique<U>&& other) noexcept
  {
    reset();
    m_Pointer = other.m_Pointer;
    m_Destroy = other.m_Destroy;
    other.m_Pointer = nullptr;
    other.m_Destroy = nullptr;
    return *this;
  }

  Unique& operator=(decltype(nullptr)) noexcept
  {
    reset();
    return *this;
  }

  void reset() noexcept
  {
    if (m_Pointer != nullptr)
      m_Destroy(m_Pointer);
    m_Pointer = nullptr;
    m_Destroy = nullptr;
  }

  [[nodiscard]] T* get() const noexcept
  {
    return m_Pointer;
  }

  [[nodiscard]] T* operator->() const noexcept
  {
    return m_Pointer;
  }

  [[nodiscard]] T& operator*() const noexcept
  {
    return *m_Pointer;
  }

  [[nodiscard]] explicit operator bool() const noexcept
  {
    return m_Pointer != nullptr;
  }

  [[nodiscard]] bool operator==(decltype(nullptr)) const noexcept
  {
    return m_Pointer == nullptr;
  }

  [[nodiscard]] bool operator!=(decltype(nullptr)) const noexcept
  {
    return m_Pointer != nullptr;
  }

private:
  T* m_Pointer {nullptr};
  void (*m_Destroy)(void*) noexcept {nullptr};
};

template <typename T>
class Shared
{
  template <typename>
  friend class Shared;

public:
  constexpr Shared() noexcept = default;
  constexpr Shared(decltype(nullptr)) noexcept
  {}

  template <typename U, typename Deleter>
    requires requires(U* value) { static_cast<T*>(value); }
  Shared(U* object, Deleter deleter) noexcept : m_Pointer(object)
  {
    using Control = detail::SharedControlWithDeleter<Deleter>;
    void* memory = AllocBytes(sizeof(Control), alignof(Control));
    m_Control = new (memory, Placement) Control(object, static_cast<Deleter&&>(deleter));
  }

  ~Shared() noexcept
  {
    reset();
  }

  Shared(const Shared& other) noexcept : m_Pointer(other.m_Pointer), m_Control(other.m_Control)
  {
    if (m_Control != nullptr)
      detail::AddReference(m_Control);
  }

  Shared(Shared&& other) noexcept : m_Pointer(other.m_Pointer), m_Control(other.m_Control)
  {
    other.m_Pointer = nullptr;
    other.m_Control = nullptr;
  }

  Shared& operator=(const Shared& other) noexcept
  {
    if (this != &other)
    {
      reset();
      m_Pointer = other.m_Pointer;
      m_Control = other.m_Control;
      if (m_Control != nullptr)
        detail::AddReference(m_Control);
    }
    return *this;
  }

  Shared& operator=(Shared&& other) noexcept
  {
    if (this != &other)
    {
      reset();
      m_Pointer = other.m_Pointer;
      m_Control = other.m_Control;
      other.m_Pointer = nullptr;
      other.m_Control = nullptr;
    }
    return *this;
  }

  Shared& operator=(decltype(nullptr)) noexcept
  {
    reset();
    return *this;
  }

  void reset() noexcept
  {
    if (m_Control != nullptr && detail::RemoveReference(m_Control))
      m_Control->Destroy(m_Control);
    m_Pointer = nullptr;
    m_Control = nullptr;
  }

  [[nodiscard]] T* get() const noexcept
  {
    return m_Pointer;
  }

  [[nodiscard]] explicit operator bool() const noexcept
  {
    return m_Pointer != nullptr;
  }

  [[nodiscard]] bool operator==(decltype(nullptr)) const noexcept
  {
    return m_Pointer == nullptr;
  }

  [[nodiscard]] bool operator!=(decltype(nullptr)) const noexcept
  {
    return m_Pointer != nullptr;
  }

private:
  T* m_Pointer {nullptr};
  detail::SharedControl* m_Control {nullptr};
};

template <typename T, typename... Args>
[[nodiscard]] Unique<T> CreateUnique(Args&&... args) noexcept
{
  void* memory = AllocBytes(sizeof(T), alignof(T));
  T* object = new (memory, Placement) T(static_cast<Args&&>(args)...);
  return Unique<T>(object, detail::DestroyObject<T>);
}

template <typename T>
[[nodiscard]] Unique<T> CreateUniqueFromRaw(T* object) noexcept
{
  return Unique<T>(object, detail::DestroyObject<T>);
}

template <typename T, typename... Args>
[[nodiscard]] Shared<T> CreateShared(Args&&... args) noexcept
{
  void* memory = AllocBytes(sizeof(T), alignof(T));
  T* object = new (memory, Placement) T(static_cast<Args&&>(args)...);
  return Shared<T>(object, detail::DestroyObject<T>);
}

template <typename T>
[[nodiscard]] Shared<T> CreateSharedFromRaw(T* object) noexcept
{
  return Shared<T>(object, detail::DestroyObject<T>);
}

}  // namespace gecko
