#pragma once

#include "gecko/core/services/jobs.h"

namespace gecko::runtime {

struct ThreadPoolState;

class ThreadPoolJobSystem final : public IJobSystem
{
public:
  ThreadPoolJobSystem() = default;
  ~ThreadPoolJobSystem() override
  {
    Shutdown();
  }

  JobHandle SubmitRaw(JobFn job, JobPriority priority = JobPriority::Normal,
                      Label label = Label {}) noexcept override;
  JobHandle SubmitRaw(JobFn job, const JobHandle* dependencies, u32 dependencyCount,
                      JobPriority priority = JobPriority::Normal, Label label = Label {}) noexcept override;
  void Wait(JobHandle handle) noexcept override;
  void WaitAll(const JobHandle* handles, u32 count) noexcept override;
  bool IsComplete(JobHandle handle) noexcept override;
  u32 WorkerThreadCount() const noexcept override;
  void ProcessJobs(u32 maxJobs = 1) noexcept override;

  bool Init() noexcept override;
  void Shutdown() noexcept override;

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
