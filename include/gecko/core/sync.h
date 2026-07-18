#pragma once

#include "gecko/core/types.h"

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace gecko {

class SpinMutex
{
public:
  void Lock() noexcept
  {
#if defined(_MSC_VER)
    while (_InterlockedExchange(reinterpret_cast<volatile long*>(&m_State), 1) != 0)
    {
      while (m_State != 0)
        _mm_pause();
    }
#else
    while (__atomic_exchange_n(&m_State, 1U, __ATOMIC_ACQUIRE) != 0)
    {
      while (__atomic_load_n(&m_State, __ATOMIC_RELAXED) != 0)
        __builtin_ia32_pause();
    }
#endif
  }

  void Unlock() noexcept
  {
#if defined(_MSC_VER)
    (void)_InterlockedExchange(reinterpret_cast<volatile long*>(&m_State), 0);
#else
    __atomic_store_n(&m_State, 0U, __ATOMIC_RELEASE);
#endif
  }

private:
  u32 m_State {0};
};

class LockGuard
{
public:
  explicit LockGuard(SpinMutex& mutex) noexcept : m_Mutex(mutex)
  {
    m_Mutex.Lock();
  }
  ~LockGuard() noexcept
  {
    m_Mutex.Unlock();
  }
  LockGuard(const LockGuard&) = delete;
  LockGuard& operator=(const LockGuard&) = delete;

private:
  SpinMutex& m_Mutex;
};

}  // namespace gecko
