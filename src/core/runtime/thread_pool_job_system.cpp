#include "private/thread_pool_job_system.h"

#include "gecko/core/assert.h"
#include "gecko/core/placement.h"
#include "gecko/core/services/memory.h"
#include "gecko/core/services/profiler.h"
#include "gecko/core/sync.h"
#include "gecko/platform/threading.h"
#include "private/labels.h"

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
  Thread Threads[MaxWorkers] {};
  Mutex JobMutex;
  ConditionVariable Wake;
  ConditionVariable Complete;
  u64 NextJobId {1};
  u32 WorkerCount {0};
  bool ShuttingDown {false};
};

namespace {

void WakeOne(ThreadPoolState& state) noexcept
{
  state.Wake.SignalOne();
}

void WakeAll(ThreadPoolState& state) noexcept
{
  state.Wake.SignalAll();
  state.Complete.SignalAll();
}

void WaitForCompletion(ThreadPoolState& state) noexcept
{
  state.Complete.Wait(state.JobMutex);
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

void ExecuteJob(ThreadPoolState& state, JobSlot& slot) noexcept
{
  {
    MemoryLabelScope memoryLabel(slot.JobLabel);
    slot.Function.Invoke(slot.Function.User);
  }
  if (slot.Function.Free != nullptr && slot.Function.User != nullptr)
    slot.Function.Free(slot.Function.User);

  LockGuard lock(state.JobMutex);
  slot.Function = {};
  slot.State = JobState::Free;
  WakeAll(state);
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
    if (!state->Threads[index].Start(
            [](void* user) noexcept {
              auto* start = static_cast<WorkerStart*>(user);
              start->Owner->WorkerThreadFunction(start->Index);
            },
            &state->Starts[index]))
      break;
    ++state->WorkerCount;
  }
  return state->WorkerCount != 0;
}

void ThreadPoolJobSystem::Shutdown() noexcept
{
  ThreadPoolState* state = m_State;
  if (state == nullptr)
    return;

  {
    LockGuard lock(state->JobMutex);
    state->ShuttingDown = true;
    WakeAll(*state);
  }

  for (u32 index = 0; index < state->WorkerCount; ++index)
    state->Threads[index].Join();

  for (JobSlot& slot : state->Jobs)
  {
    if (slot.Function.Free != nullptr && slot.Function.User != nullptr)
      slot.Function.Free(slot.Function.User);
  }
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
  JobHandle handle {};
  {
    LockGuard lock(state.JobMutex);
    JobSlot* destination = nullptr;
    for (JobSlot& slot : state.Jobs)
    {
      if (slot.State == JobState::Free)
      {
        destination = &slot;
        break;
      }
    }
    if (destination != nullptr && !state.ShuttingDown)
    {
      handle = JobHandle {state.NextJobId++};
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
    }
  }

  if (!handle.IsValid() && job.Free != nullptr && job.User != nullptr)
    job.Free(job.User);
  return handle;
}

void ThreadPoolJobSystem::Wait(JobHandle handle) noexcept
{
  if (m_State == nullptr || !handle.IsValid())
    return;
  ThreadPoolState& state = *m_State;
  LockGuard lock(state.JobMutex);
  while (FindJob(state, handle) != nullptr)
    WaitForCompletion(state);
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
  LockGuard lock(state.JobMutex);
  const bool complete = FindJob(state, handle) == nullptr;
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
  JobSlot* slot = nullptr;
  {
    LockGuard lock(state.JobMutex);
    slot = TakeReadyJob(state);
  }
  if (slot == nullptr)
    return false;

  ExecuteJob(state, *slot);
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
    JobSlot* slot = nullptr;
    {
      LockGuard lock(state.JobMutex);
      while (!state.ShuttingDown && (slot = TakeReadyJob(state)) == nullptr)
        state.Wake.Wait(state.JobMutex);
      if (state.ShuttingDown)
        return;
    }
    ExecuteJob(state, *slot);
  }
}

}  // namespace gecko::runtime
