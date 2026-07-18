#pragma once

#include "gecko/core/services/jobs.h"

namespace gecko::runtime {

struct ThreadPoolState;

class ThreadPoolJobSystem final
{
public:
  ThreadPoolJobSystem() = default;
  ~ThreadPoolJobSystem()
  {
    Shutdown();
  }

  JobHandle SubmitRaw(JobFn job, JobPriority priority = JobPriority::Normal, Label label = Label {}) noexcept;
  JobHandle SubmitRaw(JobFn job, const JobHandle* dependencies, u32 dependencyCount,
                      JobPriority priority = JobPriority::Normal, Label label = Label {}) noexcept;
  void Wait(JobHandle handle) noexcept;
  void WaitAll(const JobHandle* handles, u32 count) noexcept;
  bool IsComplete(JobHandle handle) noexcept;
  u32 WorkerThreadCount() const noexcept;
  void ProcessJobs(u32 maxJobs = 1) noexcept;

  bool Init() noexcept;
  void Shutdown() noexcept;

  void SetWorkerThreadCount(u32 count) noexcept
  {
    m_RequestedWorkerCount = count;
  }

private:
  void WorkerThreadFunction(u32 workerIndex) noexcept;
  bool RunOneJob() noexcept;

  ThreadPoolState* m_State {nullptr};
  u32 m_RequestedWorkerCount {0};
};

}  // namespace gecko::runtime
