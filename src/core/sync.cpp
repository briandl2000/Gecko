#include "gecko/core/sync.h"

#include "gecko/core/assert.h"

namespace gecko {

Semaphore::Semaphore(u32 initialCount) noexcept : m_Count(initialCount)
{}

void Semaphore::Signal(u32 count) noexcept
{
  GECKO_ASSERT(count != 0, "Semaphore signal count cannot be zero");
  LockGuard lock(m_Mutex);
  m_Count += count;
  if (count == 1)
    m_Condition.SignalOne();
  else
    m_Condition.SignalAll();
}

void Semaphore::Wait() noexcept
{
  LockGuard lock(m_Mutex);
  while (m_Count == 0)
    m_Condition.Wait(m_Mutex);
  --m_Count;
}

bool Semaphore::TryWait() noexcept
{
  LockGuard lock(m_Mutex);
  if (m_Count == 0)
    return false;
  --m_Count;
  return true;
}

}  // namespace gecko
