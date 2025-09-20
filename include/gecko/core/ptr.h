#pragma once

#include <memory>

namespace gecko
{
  template<typename T>
  using Scope = std::unique_ptr<T>;

  template<typename T, typename ... Args>
  constexpr Scope<T> CreateScope(Args&& ... args)
  {
    return std::make_unique<T>(std::forward<Args>(args)...);
  }

  template<typename T>
  constexpr Scope<T> CreateScopeFromRaw(T* t)
  {
    return std::unique_ptr<T>(t);
  }

  template<typename T>
  using Ref = std::shared_ptr<T>;

  template<typename T, typename ... Args>
  constexpr Ref<T> CreateRef(Args&& ... args)
  {
    return std::make_shared<T>(std::forward<Args>(args)...);
  }

  template<typename T>
  constexpr Ref<T> CreateRefFromRaw(T* t)
  {
    return std::shared_ptr<T>(t);
  }

  template<typename T>
  using WeakRef = std::weak_ptr<T>;

  template<typename T>
  constexpr WeakRef<T> CreateWeakRef(Ref<T> ref)
  {
    return std::weak_ptr<T>(ref);
  }
}
