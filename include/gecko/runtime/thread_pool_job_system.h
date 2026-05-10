#pragma once

/// @file
/// `ThreadPoolJobSystem` -- priority-queue + dependency-graph
/// `IJobSystem` implementation backed by a fixed worker-thread pool.

#include "gecko/core/services/jobs.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>

namespace gecko::runtime {

struct Job
{
  JobFn Function {};
  JobPriority Priority;
  Label JobLabel;
  JobHandle Handle;
  std::vector<JobHandle> Dependencies;
  std::atomic<bool> Completed {false};

  Job() = default;
  Job(JobFn func, JobPriority prio, gecko::Label label, JobHandle handle) noexcept
      : Function(func), Priority(prio), JobLabel(label), Handle(handle)
  {}

  ~Job() noexcept
  {
    // If the worker ran the job it cleared Function; otherwise we are
    // tearing down without dispatching, so release captured state.
    if (Function.Free && Function.User)
      Function.Free(Function.User);
  }

  // Make non-copyable to avoid atomic copy issues
  Job(const Job&) = delete;
  Job& operator=(const Job&) = delete;

  // Allow move operations
  Job(Job&& other) noexcept
      : Function(other.Function), Priority(other.Priority), JobLabel(other.JobLabel), Handle(other.Handle),
        Dependencies(std::move(other.Dependencies)), Completed(other.Completed.load())
  {
    other.Function = JobFn {};
  }

  Job& operator=(Job&& other) noexcept
  {
    if (this != &other)
    {
      if (Function.Free && Function.User)
        Function.Free(Function.User);
      Function = other.Function;
      other.Function = JobFn {};
      Priority = other.Priority;
      JobLabel = other.JobLabel;
      Handle = other.Handle;
      Dependencies = std::move(other.Dependencies);
      Completed.store(other.Completed.load());
    }
    return *this;
  }
};

/// Worker-pool job-system implementation. Workers pull jobs off a
/// priority queue and respect declared dependencies.
class ThreadPoolJobSystem final : public IJobSystem
{
public:
  ThreadPoolJobSystem() = default;
  virtual ~ThreadPoolJobSystem()
  {
    Shutdown();
  }

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
  void SetWorkerThreadCount(u32 count) noexcept
  {
    m_RequestedWorkerCount = count;
  }

private:
  struct JobCompare
  {
    bool operator()(const std::shared_ptr<Job>& a, const std::shared_ptr<Job>& b) const
    {
      // Higher priority jobs should come first (reverse order for
      // priority_queue)
      return static_cast<int>(a->Priority) < static_cast<int>(b->Priority);
    }
  };

  void WorkerThreadFunction(u32 workerIndex) noexcept;
  std::shared_ptr<Job> GetNextReadyJob() noexcept;
  bool AreJobDependenciesComplete(const std::shared_ptr<Job>& job) noexcept;
  JobHandle GenerateJobHandle() noexcept;

  mutable std::mutex m_Mutex;
  std::condition_variable m_JobAvailable;
  std::condition_variable m_JobCompleted;

  std::priority_queue<std::shared_ptr<Job>, std::vector<std::shared_ptr<Job>>, JobCompare> m_JobQueue;
  std::unordered_map<u64, std::shared_ptr<Job>> m_ActiveJobs;

  std::vector<std::thread> m_WorkerThreads;
  std::atomic<bool> m_Shutdown {false};
  std::atomic<u64> m_NextJobId {1};

  u32 m_RequestedWorkerCount {0};  // 0 = auto-detect
  bool m_Initialized {false};
};

}  // namespace gecko::runtime
