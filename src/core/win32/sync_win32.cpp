#if defined(GECKO_PLATFORM_WINDOWS)

#include "gecko/core/sync.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

namespace gecko {

namespace {

using NativeMutex = SRWLOCK;
using NativeCondition = CONDITION_VARIABLE;

static_assert(sizeof(NativeMutex) <= 64);
static_assert(alignof(NativeMutex) <= 8);
static_assert(sizeof(NativeCondition) <= 64);
static_assert(alignof(NativeCondition) <= 8);

}  // namespace

Mutex::Mutex() noexcept
{
  ::InitializeSRWLock(reinterpret_cast<NativeMutex*>(m_Storage));
}

Mutex::~Mutex() noexcept = default;

void Mutex::Lock() noexcept
{
  ::AcquireSRWLockExclusive(reinterpret_cast<NativeMutex*>(m_Storage));
}

bool Mutex::TryLock() noexcept
{
  return ::TryAcquireSRWLockExclusive(reinterpret_cast<NativeMutex*>(m_Storage)) != 0;
}

void Mutex::Unlock() noexcept
{
  ::ReleaseSRWLockExclusive(reinterpret_cast<NativeMutex*>(m_Storage));
}

ConditionVariable::ConditionVariable() noexcept
{
  ::InitializeConditionVariable(reinterpret_cast<NativeCondition*>(m_Storage));
}

ConditionVariable::~ConditionVariable() noexcept = default;

void ConditionVariable::Wait(Mutex& mutex) noexcept
{
  (void)::SleepConditionVariableSRW(reinterpret_cast<NativeCondition*>(m_Storage),
                                    reinterpret_cast<NativeMutex*>(mutex.m_Storage), INFINITE, 0);
}

bool ConditionVariable::WaitFor(Mutex& mutex, u64 nanoseconds) noexcept
{
  const u64 milliseconds = (nanoseconds + 999'999ULL) / 1'000'000ULL;
  const DWORD timeout = milliseconds > MAXDWORD ? MAXDWORD : static_cast<DWORD>(milliseconds);
  if (::SleepConditionVariableSRW(reinterpret_cast<NativeCondition*>(m_Storage),
                                  reinterpret_cast<NativeMutex*>(mutex.m_Storage), timeout, 0) != 0)
    return true;
  return ::GetLastError() != ERROR_TIMEOUT;
}

void ConditionVariable::SignalOne() noexcept
{
  ::WakeConditionVariable(reinterpret_cast<NativeCondition*>(m_Storage));
}

void ConditionVariable::SignalAll() noexcept
{
  ::WakeAllConditionVariable(reinterpret_cast<NativeCondition*>(m_Storage));
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

  HANDLE handle = ::CreateThread(
      nullptr, 0,
      [](void* threadValue) -> DWORD {
        auto* thread = static_cast<Thread*>(threadValue);
        thread->m_Function(thread->m_User);
        return 0;
      },
      this, 0, nullptr);
  if (handle == nullptr)
  {
    m_Function = nullptr;
    m_User = nullptr;
    return false;
  }
  m_Handle = reinterpret_cast<u64>(handle);
  m_Joinable = true;
  return true;
}

void Thread::Join() noexcept
{
  if (!m_Joinable)
    return;
  HANDLE handle = reinterpret_cast<HANDLE>(m_Handle);
  (void)::WaitForSingleObject(handle, INFINITE);
  (void)::CloseHandle(handle);
  m_Function = nullptr;
  m_User = nullptr;
  m_Handle = 0;
  m_Joinable = false;
}

}  // namespace gecko

#endif
