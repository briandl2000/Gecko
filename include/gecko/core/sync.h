#pragma once

#include "gecko/api.h"
#include "gecko/core/atomic.h"
#include "gecko/core/types.h"

namespace gecko {

/// Busy-wait lock for very short critical sections. Prefer Mutex whenever a
/// lock may be held while waiting on the OS, doing I/O, or running callbacks.
class SpinMutex
{
public:
  void Lock() noexcept
  {
    while (m_State.Exchange(1) != 0)
      while (m_State.Load() != 0)
        CpuRelax();
  }

  [[nodiscard]] bool TryLock() noexcept
  {
    return m_State.Exchange(1) == 0;
  }

  void Unlock() noexcept
  {
    m_State.Store(0);
  }

private:
  AtomicU32 m_State;
};

/// Sleeping, OS-backed mutual exclusion primitive. Native representation is
/// kept in fixed local storage so the public header remains platform-neutral.
class GECKO_API Mutex
{
public:
  Mutex() noexcept;
  ~Mutex() noexcept;

  Mutex(const Mutex&) = delete;
  Mutex& operator=(const Mutex&) = delete;

  void Lock() noexcept;
  [[nodiscard]] bool TryLock() noexcept;
  void Unlock() noexcept;

private:
  friend class ConditionVariable;
  static constexpr usize StorageSize = 64;
  alignas(8) byte m_Storage[StorageSize] {};
};

/// Condition variable used with a locked Mutex. Wait atomically releases the
/// mutex while sleeping and reacquires it before returning.
class GECKO_API ConditionVariable
{
public:
  ConditionVariable() noexcept;
  ~ConditionVariable() noexcept;

  ConditionVariable(const ConditionVariable&) = delete;
  ConditionVariable& operator=(const ConditionVariable&) = delete;

  void Wait(Mutex& mutex) noexcept;
  [[nodiscard]] bool WaitFor(Mutex& mutex, u64 nanoseconds) noexcept;
  void SignalOne() noexcept;
  void SignalAll() noexcept;

private:
  static constexpr usize StorageSize = 64;
  alignas(8) byte m_Storage[StorageSize] {};
};

/// Counting semaphore implemented from Gecko's mutex and condition variable.
class GECKO_API Semaphore
{
public:
  explicit Semaphore(u32 initialCount = 0) noexcept;

  Semaphore(const Semaphore&) = delete;
  Semaphore& operator=(const Semaphore&) = delete;

  void Signal(u32 count = 1) noexcept;
  void Wait() noexcept;
  [[nodiscard]] bool TryWait() noexcept;

private:
  Mutex m_Mutex;
  ConditionVariable m_Condition;
  u32 m_Count {0};
};

using ThreadFunction = void (*)(void* user) noexcept;

/// Joinable native thread with one platform-neutral entry point.
class GECKO_API Thread
{
public:
  Thread() noexcept = default;
  ~Thread() noexcept;

  Thread(const Thread&) = delete;
  Thread& operator=(const Thread&) = delete;

  [[nodiscard]] bool Start(ThreadFunction function, void* user = nullptr) noexcept;
  void Join() noexcept;
  [[nodiscard]] bool IsJoinable() const noexcept
  {
    return m_Joinable;
  }

private:
  ThreadFunction m_Function {nullptr};
  void* m_User {nullptr};
  u64 m_Handle {0};
  bool m_Joinable {false};
};

/// RAII lock that works with Mutex, SpinMutex, or another Lock/Unlock type.
template <typename MutexType>
class LockGuard
{
public:
  explicit LockGuard(MutexType& mutex) noexcept : m_Mutex(mutex)
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
  MutexType& m_Mutex;
};

}  // namespace gecko
