#if defined(GECKO_PLATFORM_LINUX)

#include "gecko/core/sync.h"

#include <errno.h>
#include <pthread.h>
#include <time.h>

namespace gecko {

namespace {

using NativeMutex = pthread_mutex_t;
using NativeCondition = pthread_cond_t;

static_assert(sizeof(NativeMutex) <= 64);
static_assert(alignof(NativeMutex) <= 8);
static_assert(sizeof(NativeCondition) <= 64);
static_assert(alignof(NativeCondition) <= 8);

}  // namespace

Mutex::Mutex() noexcept
{
  (void)::pthread_mutex_init(reinterpret_cast<NativeMutex*>(m_Storage), nullptr);
}

Mutex::~Mutex() noexcept
{
  (void)::pthread_mutex_destroy(reinterpret_cast<NativeMutex*>(m_Storage));
}

void Mutex::Lock() noexcept
{
  (void)::pthread_mutex_lock(reinterpret_cast<NativeMutex*>(m_Storage));
}

bool Mutex::TryLock() noexcept
{
  return ::pthread_mutex_trylock(reinterpret_cast<NativeMutex*>(m_Storage)) == 0;
}

void Mutex::Unlock() noexcept
{
  (void)::pthread_mutex_unlock(reinterpret_cast<NativeMutex*>(m_Storage));
}

ConditionVariable::ConditionVariable() noexcept
{
  pthread_condattr_t attributes {};
  (void)::pthread_condattr_init(&attributes);
  (void)::pthread_condattr_setclock(&attributes, CLOCK_MONOTONIC);
  (void)::pthread_cond_init(reinterpret_cast<NativeCondition*>(m_Storage), &attributes);
  (void)::pthread_condattr_destroy(&attributes);
}

ConditionVariable::~ConditionVariable() noexcept
{
  (void)::pthread_cond_destroy(reinterpret_cast<NativeCondition*>(m_Storage));
}

void ConditionVariable::Wait(Mutex& mutex) noexcept
{
  (void)::pthread_cond_wait(reinterpret_cast<NativeCondition*>(m_Storage),
                            reinterpret_cast<NativeMutex*>(mutex.m_Storage));
}

bool ConditionVariable::WaitFor(Mutex& mutex, u64 nanoseconds) noexcept
{
  timespec deadline {};
  (void)::clock_gettime(CLOCK_MONOTONIC, &deadline);
  const u64 seconds = nanoseconds / 1'000'000'000ULL;
  const u64 remainder = nanoseconds % 1'000'000'000ULL;
  deadline.tv_sec += static_cast<time_t>(seconds);
  deadline.tv_nsec += static_cast<long>(remainder);
  if (deadline.tv_nsec >= 1'000'000'000L)
  {
    ++deadline.tv_sec;
    deadline.tv_nsec -= 1'000'000'000L;
  }
  return ::pthread_cond_timedwait(reinterpret_cast<NativeCondition*>(m_Storage),
                                  reinterpret_cast<NativeMutex*>(mutex.m_Storage), &deadline) != ETIMEDOUT;
}

void ConditionVariable::SignalOne() noexcept
{
  (void)::pthread_cond_signal(reinterpret_cast<NativeCondition*>(m_Storage));
}

void ConditionVariable::SignalAll() noexcept
{
  (void)::pthread_cond_broadcast(reinterpret_cast<NativeCondition*>(m_Storage));
}

Thread::~Thread() noexcept
{
  Join();
}

bool Thread::Start(ThreadFunction function, void* user) noexcept
{
  if (m_Joinable || function == nullptr)
    return false;
  m_Function = function;
  m_User = user;

  pthread_t handle {};
  const int result = ::pthread_create(
      &handle, nullptr,
      [](void* threadValue) -> void* {
        auto* thread = static_cast<Thread*>(threadValue);
        thread->m_Function(thread->m_User);
        return nullptr;
      },
      this);
  if (result != 0)
  {
    m_Function = nullptr;
    m_User = nullptr;
    return false;
  }
  static_assert(sizeof(handle) <= sizeof(m_Handle));
  __builtin_memcpy(&m_Handle, &handle, sizeof(handle));
  m_Joinable = true;
  return true;
}

void Thread::Join() noexcept
{
  if (!m_Joinable)
    return;
  pthread_t handle {};
  __builtin_memcpy(&handle, &m_Handle, sizeof(handle));
  (void)::pthread_join(handle, nullptr);
  m_Function = nullptr;
  m_User = nullptr;
  m_Handle = 0;
  m_Joinable = false;
}

}  // namespace gecko

#endif
