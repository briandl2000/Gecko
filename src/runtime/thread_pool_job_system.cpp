#include "private/thread_pool_job_system.h"

#include "gecko/core/assert.h"
#include "gecko/core/placement.h"
#include "gecko/core/services/memory.h"
#include "gecko/core/services/profiler.h"
#include "gecko/platform/threading.h"
#include "private/labels.h"

#if defined(GECKO_PLATFORM_WINDOWS)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#elif defined(GECKO_PLATFORM_LINUX)
#include <pthread.h>
#endif

namespace gecko::runtime {

constexpr u32 MaxWorkers = 16;
constexpr u32 MaxJobs = 4096;
constexpr u32 MaxDependencies = 16;

enum class JobState : u8
{
  Free,
  Queued,
  Running,
};

struct JobSlot
{
  JobFn Function {};
  JobHandle Handle {};
  JobHandle Dependencies[MaxDependencies] {};
  Label JobLabel {};
  u32 DependencyCount {0};
  JobPriority Priority {JobPriority::Normal};
  JobState State {JobState::Free};
};

struct WorkerStart
{
  ThreadPoolJobSystem* Owner {nullptr};
  u32 Index {0};
};

struct ThreadPoolState
{
  JobSlot Jobs[MaxJobs] {};
  WorkerStart Starts[MaxWorkers] {};
#if defined(GECKO_PLATFORM_WINDOWS)
  HANDLE Threads[MaxWorkers] {};
  SRWLOCK Mutex = SRWLOCK_INIT;
  CONDITION_VARIABLE Wake = CONDITION_VARIABLE_INIT;
  CONDITION_VARIABLE Complete = CONDITION_VARIABLE_INIT;
#elif defined(GECKO_PLATFORM_LINUX)
  pthread_t Threads[MaxWorkers] {};
  pthread_mutex_t Mutex = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t Wake = PTHREAD_COND_INITIALIZER;
  pthread_cond_t Complete = PTHREAD_COND_INITIALIZER;
#endif
  u64 NextJobId {1};
  u32 WorkerCount {0};
  bool ShuttingDown {false};
};

namespace {

void Lock(ThreadPoolState& state) noexcept
{
#if defined(GECKO_PLATFORM_WINDOWS)
  ::AcquireSRWLockExclusive(&state.Mutex);
#elif defined(GECKO_PLATFORM_LINUX)
  (void)::pthread_mutex_lock(&state.Mutex);
#endif
}

void Unlock(ThreadPoolState& state) noexcept
{
#if defined(GECKO_PLATFORM_WINDOWS)
  ::ReleaseSRWLockExclusive(&state.Mutex);
#elif defined(GECKO_PLATFORM_LINUX)
  (void)::pthread_mutex_unlock(&state.Mutex);
#endif
}

void WakeOne(ThreadPoolState& state) noexcept
{
#if defined(GECKO_PLATFORM_WINDOWS)
  ::WakeConditionVariable(&state.Wake);
#elif defined(GECKO_PLATFORM_LINUX)
  (void)::pthread_cond_signal(&state.Wake);
#endif
}

void WakeAll(ThreadPoolState& state) noexcept
{
#if defined(GECKO_PLATFORM_WINDOWS)
  ::WakeAllConditionVariable(&state.Wake);
  ::WakeAllConditionVariable(&state.Complete);
#elif defined(GECKO_PLATFORM_LINUX)
  (void)::pthread_cond_broadcast(&state.Wake);
  (void)::pthread_cond_broadcast(&state.Complete);
#endif
}

void SignalComplete(ThreadPoolState& state) noexcept
{
#if defined(GECKO_PLATFORM_WINDOWS)
  ::WakeAllConditionVariable(&state.Complete);
#elif defined(GECKO_PLATFORM_LINUX)
  (void)::pthread_cond_broadcast(&state.Complete);
#endif
}

void WaitForWake(ThreadPoolState& state) noexcept
{
#if defined(GECKO_PLATFORM_WINDOWS)
  (void)::SleepConditionVariableSRW(&state.Wake, &state.Mutex, 100, 0);
#elif defined(GECKO_PLATFORM_LINUX)
  timespec deadline {};
  (void)::clock_gettime(CLOCK_REALTIME, &deadline);
  deadline.tv_nsec += 100'000'000;
  if (deadline.tv_nsec >= 1'000'000'000)
  {
    ++deadline.tv_sec;
    deadline.tv_nsec -= 1'000'000'000;
  }
  (void)::pthread_cond_timedwait(&state.Wake, &state.Mutex, &deadline);
#endif
}

void WaitForCompletion(ThreadPoolState& state) noexcept
{
#if defined(GECKO_PLATFORM_WINDOWS)
  (void)::SleepConditionVariableSRW(&state.Complete, &state.Mutex, INFINITE, 0);
#elif defined(GECKO_PLATFORM_LINUX)
  (void)::pthread_cond_wait(&state.Complete, &state.Mutex);
#endif
}

JobSlot* FindJob(ThreadPoolState& state, JobHandle handle) noexcept
{
  for (JobSlot& slot : state.Jobs)
  {
    if (slot.State != JobState::Free && slot.Handle == handle)
      return &slot;
  }
  return nullptr;
}

bool DependenciesComplete(ThreadPoolState& state, const JobSlot& slot) noexcept
{
  for (u32 index = 0; index < slot.DependencyCount; ++index)
  {
    if (FindJob(state, slot.Dependencies[index]) != nullptr)
      return false;
  }
  return true;
}

JobSlot* TakeReadyJob(ThreadPoolState& state) noexcept
{
  JobSlot* best = nullptr;
  for (JobSlot& slot : state.Jobs)
  {
    if (slot.State != JobState::Queued || !DependenciesComplete(state, slot))
      continue;
    if (best == nullptr || static_cast<u8>(slot.Priority) > static_cast<u8>(best->Priority))
      best = &slot;
  }
  if (best != nullptr)
    best->State = JobState::Running;
  return best;
}

}  // namespace

bool ThreadPoolJobSystem::Init() noexcept
{
  GECKO_ASSERT(m_State == nullptr, "ThreadPoolJobSystem already initialized");
  void* storage = AllocBytes(sizeof(ThreadPoolState), alignof(ThreadPoolState));
  auto* state = new (storage, Placement) ThreadPoolState {};

  u32 workerCount = m_RequestedWorkerCount == 0 ? platform::HardwareThreadCount() : m_RequestedWorkerCount;
  if (workerCount == 0)
    workerCount = 1;
  if (workerCount > MaxWorkers)
    workerCount = MaxWorkers;

  m_State = state;
  for (u32 index = 0; index < workerCount; ++index)
  {
    state->Starts[index] = WorkerStart {.Owner = this, .Index = index};
#if defined(GECKO_PLATFORM_WINDOWS)
    state->Threads[index] = ::CreateThread(
        nullptr, 0,
        [](void* user) -> DWORD {
          auto* start = static_cast<WorkerStart*>(user);
          start->Owner->WorkerThreadFunction(start->Index);
          return 0;
        },
        &state->Starts[index], 0, nullptr);
    if (state->Threads[index] == nullptr)
      break;
#elif defined(GECKO_PLATFORM_LINUX)
    const int result = ::pthread_create(
        &state->Threads[index], nullptr,
        [](void* user) -> void* {
          auto* start = static_cast<WorkerStart*>(user);
          start->Owner->WorkerThreadFunction(start->Index);
          return nullptr;
        },
        &state->Starts[index]);
    if (result != 0)
      break;
#endif
    ++state->WorkerCount;
  }
  return state->WorkerCount != 0;
}

void ThreadPoolJobSystem::Shutdown() noexcept
{
  ThreadPoolState* state = m_State;
  if (state == nullptr)
    return;

  Lock(*state);
  state->ShuttingDown = true;
  WakeAll(*state);
  Unlock(*state);

  for (u32 index = 0; index < state->WorkerCount; ++index)
  {
#if defined(GECKO_PLATFORM_WINDOWS)
    (void)::WaitForSingleObject(state->Threads[index], INFINITE);
    (void)::CloseHandle(state->Threads[index]);
#elif defined(GECKO_PLATFORM_LINUX)
    (void)::pthread_join(state->Threads[index], nullptr);
#endif
  }

  for (JobSlot& slot : state->Jobs)
  {
    if (slot.Function.Free != nullptr && slot.Function.User != nullptr)
      slot.Function.Free(slot.Function.User);
  }
#if defined(GECKO_PLATFORM_LINUX)
  (void)::pthread_cond_destroy(&state->Complete);
  (void)::pthread_cond_destroy(&state->Wake);
  (void)::pthread_mutex_destroy(&state->Mutex);
#endif
  state->~ThreadPoolState();
  DeallocBytes(state);
  m_State = nullptr;
}

JobHandle ThreadPoolJobSystem::SubmitRaw(JobFn job, JobPriority priority, Label label) noexcept
{
  return SubmitRaw(job, nullptr, 0, priority, label);
}

JobHandle ThreadPoolJobSystem::SubmitRaw(JobFn job, const JobHandle* dependencies, u32 dependencyCount,
                                         JobPriority priority, Label label) noexcept
{
  if (m_State == nullptr || !job.IsValid() || dependencyCount > MaxDependencies ||
      (dependencyCount != 0 && dependencies == nullptr))
  {
    if (job.Free != nullptr && job.User != nullptr)
      job.Free(job.User);
    return {};
  }

  ThreadPoolState& state = *m_State;
  Lock(state);
  JobSlot* destination = nullptr;
  for (JobSlot& slot : state.Jobs)
  {
    if (slot.State == JobState::Free)
    {
      destination = &slot;
      break;
    }
  }
  if (destination == nullptr || state.ShuttingDown)
  {
    Unlock(state);
    if (job.Free != nullptr && job.User != nullptr)
      job.Free(job.User);
    return {};
  }

  const JobHandle handle {state.NextJobId++};
  *destination = JobSlot {
      .Function = job,
      .Handle = handle,
      .JobLabel = label,
      .DependencyCount = dependencyCount,
      .Priority = priority,
      .State = JobState::Queued,
  };
  for (u32 index = 0; index < dependencyCount; ++index)
    destination->Dependencies[index] = dependencies[index];
  WakeOne(state);
  Unlock(state);
  return handle;
}

void ThreadPoolJobSystem::Wait(JobHandle handle) noexcept
{
  if (m_State == nullptr || !handle.IsValid())
    return;
  ThreadPoolState& state = *m_State;
  Lock(state);
  while (FindJob(state, handle) != nullptr)
    WaitForCompletion(state);
  Unlock(state);
}

void ThreadPoolJobSystem::WaitAll(const JobHandle* handles, u32 count) noexcept
{
  if (m_State == nullptr || handles == nullptr)
    return;
  for (u32 index = 0; index < count; ++index)
    Wait(handles[index]);
}

bool ThreadPoolJobSystem::IsComplete(JobHandle handle) noexcept
{
  if (m_State == nullptr || !handle.IsValid())
    return true;
  ThreadPoolState& state = *m_State;
  Lock(state);
  const bool complete = FindJob(state, handle) == nullptr;
  Unlock(state);
  return complete;
}

u32 ThreadPoolJobSystem::WorkerThreadCount() const noexcept
{
  return m_State != nullptr ? m_State->WorkerCount : 0;
}

void ThreadPoolJobSystem::ProcessJobs(u32 maxJobs) noexcept
{
  for (u32 index = 0; index < maxJobs && RunOneJob(); ++index)
  {}
}

bool ThreadPoolJobSystem::RunOneJob() noexcept
{
  if (m_State == nullptr)
    return false;
  ThreadPoolState& state = *m_State;
  Lock(state);
  JobSlot* slot = TakeReadyJob(state);
  Unlock(state);
  if (slot == nullptr)
    return false;

  PushMemoryLabel(slot->JobLabel);
  slot->Function.Invoke(slot->Function.User);
  PopMemoryLabel();
  if (slot->Function.Free != nullptr && slot->Function.User != nullptr)
    slot->Function.Free(slot->Function.User);

  Lock(state);
  slot->Function = {};
  slot->State = JobState::Free;
  SignalComplete(state);
  WakeAll(state);
  Unlock(state);
  return true;
}

void ThreadPoolJobSystem::WorkerThreadFunction(u32 workerIndex) noexcept
{
  static constexpr const char* WorkerNames[MaxWorkers] = {
      "job-worker-0",  "job-worker-1",  "job-worker-2",  "job-worker-3",  "job-worker-4",  "job-worker-5",
      "job-worker-6",  "job-worker-7",  "job-worker-8",  "job-worker-9",  "job-worker-10", "job-worker-11",
      "job-worker-12", "job-worker-13", "job-worker-14", "job-worker-15",
  };
  SetThreadProfilerName(WorkerNames[workerIndex]);

  ThreadPoolState& state = *m_State;
  for (;;)
  {
    if (RunOneJob())
      continue;
    Lock(state);
    if (state.ShuttingDown)
    {
      Unlock(state);
      break;
    }
    WaitForWake(state);
    Unlock(state);
  }
}

}  // namespace gecko::runtime
