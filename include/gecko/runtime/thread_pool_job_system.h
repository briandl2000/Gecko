#pragma once

/// @file
/// `ThreadPoolJobSystem` -- priority-queue + dependency-graph
/// `IJobSystem` implementation backed by a fixed worker-thread pool.

#include "gecko/core/ptr.h"
#include "gecko/core/services/jobs.h"

namespace gecko::runtime {

/// Worker-pool job-system implementation. Workers pull jobs off a
/// priority queue and respect declared dependencies.
class ThreadPoolJobSystem final : public IJobSystem
{
public:
  ThreadPoolJobSystem() noexcept;
  virtual ~ThreadPoolJobSystem();

  virtual JobHandle SubmitRaw(JobFn job, JobPriority priority = JobPriority::Normal,
                              Label label = Label {}) noexcept override;
  virtual JobHandle SubmitRaw(JobFn job, const JobHandle* dependencies, u32 dependencyCount,
                              JobPriority priority = JobPriority::Normal, Label label = Label {}) noexcept override;
  virtual void Wait(JobHandle handle) noexcept override;
  virtual void WaitAll(const JobHandle* handles, u32 count) noexcept override;
  virtual bool IsComplete(JobHandle handle) noexcept override;
  virtual u32 WorkerThreadCount() const noexcept override;
  virtual void ProcessJobs(u32 maxJobs = 1) noexcept override;

  virtual bool Init() noexcept override;
  virtual void Shutdown() noexcept override;

  /// Override the worker-thread count. Pass `0` (the default) to
  /// auto-detect from the platform's hardware-thread count. Must be
  /// called before `Init()` to take effect.
  void SetWorkerThreadCount(u32 count) noexcept;

private:
  struct Impl;

  void WorkerThreadFunction(u32 workerIndex) noexcept;
  JobHandle GenerateJobHandle() noexcept;

  ::gecko::Unique<Impl> m_Impl;
};

}  // namespace gecko::runtime
