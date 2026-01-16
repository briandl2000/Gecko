#pragma once

#include "gecko/core/jobs.h"

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
  JobFunction Function;
  JobPriority Priority;
  Label JobLabel;
  JobHandle Handle;
  std::vector<JobHandle> Dependencies;
  std::atomic<bool> Completed {false};

  Job() = default;
  Job(JobFunction func, JobPriority prio, gecko::Label label,
      JobHandle handle) noexcept
      : Function(std::move(func)), Priority(prio), JobLabel(label),
        Handle(handle)
  {}

  // Make non-copyable to avoid atomic copy issues
  Job(const Job&) = delete;
  Job& operator=(const Job&) = delete;

  // Allow move operations
  Job(Job&& other) noexcept
      : Function(std::move(other.Function)), Priority(other.Priority),
        JobLabel(other.JobLabel), Handle(other.Handle),
        Dependencies(std::move(other.Dependencies)),
        Completed(other.Completed.load())
  {}

  Job& operator=(Job&& other) noexcept
  {
    if (this != &other)
    {
      Function = std::move(other.Function);
      Priority = other.Priority;
      JobLabel = other.JobLabel;
      Handle = other.Handle;
      Dependencies = std::move(other.Dependencies);
      Completed.store(other.Completed.load());
    }
    return *this;
  }
};

class ThreadPoolJobSystem final : public IJobSystem
{
public:
  ThreadPoolJobSystem() = default;
  virtual ~ThreadPoolJobSystem() = default;

  virtual JobHandle Submit(JobFunction job,
                           JobPriority priority = JobPriority::Normal,
                           Label label = Label {}) noexcept override;
  virtual JobHandle Submit(JobFunction job, const JobHandle* dependencies,
                           u32 dependencyCount,
                           JobPriority priority = JobPriority::Normal,
                           Label label = Label {}) noexcept override;
  virtual void Wait(JobHandle handle) noexcept override;
  virtual void WaitAll(const JobHandle* handles, u32 count) noexcept override;
  virtual bool IsComplete(JobHandle handle) noexcept override;
  virtual u32 WorkerThreadCount() const noexcept override;
  virtual void ProcessJobs(u32 maxJobs = 1) noexcept override;

  virtual bool Init() noexcept override;
  virtual void Shutdown() noexcept override;

  void SetWorkerThreadCount(u32 count) noexcept
  {
    m_RequestedWorkerCount = count;
  }

private:
  struct JobCompare
  {
    bool operator()(const std::shared_ptr<Job>& a,
                    const std::shared_ptr<Job>& b) const
    {
      // Higher priority jobs should come first (reverse order for
      // priority_queue)
      return static_cast<int>(a->Priority) < static_cast<int>(b->Priority);
    }
  };

  void WorkerThreadFunction() noexcept;
  std::shared_ptr<Job> GetNextReadyJob() noexcept;
  bool AreJobDependenciesComplete(const std::shared_ptr<Job>& job) noexcept;
  JobHandle GenerateJobHandle() noexcept;

  mutable std::mutex m_Mutex;
  std::condition_variable m_JobAvailable;
  std::condition_variable m_JobCompleted;

  std::priority_queue<std::shared_ptr<Job>, std::vector<std::shared_ptr<Job>>,
                      JobCompare>
      m_JobQueue;
  std::unordered_map<u64, std::shared_ptr<Job>> m_ActiveJobs;

  std::vector<std::thread> m_WorkerThreads;
  std::atomic<bool> m_Shutdown {false};
  std::atomic<u64> m_NextJobId {1};

  u32 m_RequestedWorkerCount {0};  // 0 = auto-detect
  bool m_Initialized {false};
};

}  // namespace gecko::runtime
