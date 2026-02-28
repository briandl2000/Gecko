#include "gecko/runtime/thread_pool_job_system.h"

#include "gecko/core/assert.h"
#include "gecko/core/services/log.h"
#include "gecko/core/services/profiler.h"
#include "private/labels.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <thread>

namespace gecko::runtime {

bool ThreadPoolJobSystem::Init() noexcept
{
  // NOTE: Cannot profile Init() - profiler is initialized AFTER job system
  GECKO_ASSERT(!m_Initialized && "ThreadPoolJobSystem already initialized");

  // Determine worker thread count
  u32 workerCount = m_RequestedWorkerCount;
  if (workerCount == 0)
  {
    workerCount = std::max(1u, std::thread::hardware_concurrency());
  }

  try
  {
    m_WorkerThreads.reserve(workerCount);
    for (u32 i = 0; i < workerCount; ++i)
    {
      m_WorkerThreads.emplace_back(&ThreadPoolJobSystem::WorkerThreadFunction,
                                   this);
    }

    m_Initialized = true;
    return true;
  }
  catch (...)
  {
    // Use direct fprintf during Init() since Logger may not be available yet
    std::fprintf(
        stderr,
        "[Gecko] Failed to create worker threads for ThreadPoolJobSystem\n");
    Shutdown();
    return false;
  }
}

void ThreadPoolJobSystem::Shutdown() noexcept
{
  if (!m_Initialized)
    return;

  m_Shutdown.store(true, std::memory_order_release);
  m_JobAvailable.notify_all();

  for (auto& thread : m_WorkerThreads)
  {
    if (thread.joinable())
      thread.join();
  }

  // Force complete deallocation before allocator shutdown
  decltype(m_WorkerThreads)().swap(m_WorkerThreads);

  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    decltype(m_JobQueue)().swap(m_JobQueue);
    decltype(m_ActiveJobs)().swap(m_ActiveJobs);
  }

  m_Initialized = false;
}

JobHandle ThreadPoolJobSystem::Submit(JobFunction job, JobPriority priority,
                                      Label label) noexcept
{
  // NOTE: Cannot use profiling/logging - JobSystem is Level 1, comes before
  // Profiler (Level 2) and Logger (Level 3)

  if (!m_Initialized || !job)
  {
    return JobHandle {};
  }

  JobHandle handle = GenerateJobHandle();
  auto jobPtr = std::make_shared<Job>(std::move(job), priority, label, handle);

  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_ActiveJobs[handle.Id] = jobPtr;
    m_JobQueue.push(jobPtr);
  }

  m_JobAvailable.notify_one();
  return handle;
}

JobHandle ThreadPoolJobSystem::Submit(JobFunction job,
                                      const JobHandle* dependencies,
                                      u32 dependencyCount, JobPriority priority,
                                      Label label) noexcept
{
  // NOTE: Cannot use profiling/logging - dependency order violation

  if (!m_Initialized || !job)
  {
    return JobHandle {};
  }

  JobHandle handle = GenerateJobHandle();
  auto jobPtr = std::make_shared<Job>(std::move(job), priority, label, handle);

  // Copy dependencies
  if (dependencies && dependencyCount > 0)
  {
    jobPtr->Dependencies.reserve(dependencyCount);
    for (u32 i = 0; i < dependencyCount; ++i)
    {
      if (dependencies[i].IsValid())
      {
        jobPtr->Dependencies.push_back(dependencies[i]);
      }
    }
  }

  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_ActiveJobs[handle.Id] = jobPtr;
    m_JobQueue.push(jobPtr);
  }

  m_JobAvailable.notify_one();
  return handle;
}

void ThreadPoolJobSystem::Wait(JobHandle handle) noexcept
{
  if (!handle.IsValid())
    return;

  std::unique_lock<std::mutex> lock(m_Mutex);
  m_JobCompleted.wait(lock, [this, handle]() {
    auto it = m_ActiveJobs.find(handle.Id);
    return it == m_ActiveJobs.end() ||
           it->second->Completed.load(std::memory_order_acquire);
  });
}

void ThreadPoolJobSystem::WaitAll(const JobHandle* handles, u32 count) noexcept
{
  if (!handles || count == 0)
    return;

  std::unique_lock<std::mutex> lock(m_Mutex);
  m_JobCompleted.wait(lock, [this, handles, count]() {
    for (u32 i = 0; i < count; ++i)
    {
      if (!handles[i].IsValid())
        continue;

      auto it = m_ActiveJobs.find(handles[i].Id);
      if (it != m_ActiveJobs.end() &&
          !it->second->Completed.load(std::memory_order_acquire))
      {
        return false;
      }
    }
    return true;
  });
}

bool ThreadPoolJobSystem::IsComplete(JobHandle handle) noexcept
{
  if (!handle.IsValid())
    return true;

  // Fast path: try to find the job and check completion without holding
  // the lock for the entire duration. We use a lock to safely get the
  // shared_ptr, then check the atomic outside the lock.
  std::shared_ptr<Job> job;
  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    auto it = m_ActiveJobs.find(handle.Id);
    if (it == m_ActiveJobs.end())
      return true;  // Job not found = completed and cleaned up
    job = it->second;
  }

  // Check completion flag outside the lock
  return job->Completed.load(std::memory_order_acquire);
}

u32 ThreadPoolJobSystem::WorkerThreadCount() const noexcept
{
  return static_cast<u32>(m_WorkerThreads.size());
}

void ThreadPoolJobSystem::ProcessJobs(u32 maxJobs) noexcept
{
  for (u32 processed = 0; processed < maxJobs; ++processed)
  {
    auto job = GetNextReadyJob();
    if (!job)
      break;

    try
    {
      job->Function();
      job->Completed.store(true, std::memory_order_release);
    }
    catch (...)
    {
      job->Completed.store(true, std::memory_order_release);
    }

    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      m_ActiveJobs.erase(job->Handle.Id);
    }

    m_JobCompleted.notify_all();
  }
}

void ThreadPoolJobSystem::WorkerThreadFunction() noexcept
{
  // NOTE: Cannot use profiling/logging - JobSystem is Layer 1, comes before
  // Profiler (Layer 2) and Logger (Layer 3)

  while (!m_Shutdown.load(std::memory_order_acquire))
  {
    auto job = GetNextReadyJob();
    if (!job)
    {
      // No jobs available, wait for notification
      std::unique_lock<std::mutex> lock(m_Mutex);
      m_JobAvailable.wait_for(lock, std::chrono::milliseconds(100), [this]() {
        return m_Shutdown.load(std::memory_order_acquire) ||
               !m_JobQueue.empty();
      });
      continue;
    }

    try
    {
      // Don't profile individual jobs in worker thread - too much overhead
      // during init
      job->Function();
      job->Completed.store(true, std::memory_order_release);
    }
    catch (...)
    {
      job->Completed.store(true, std::memory_order_release);
    }

    {
      std::lock_guard<std::mutex> lock(m_Mutex);
      m_ActiveJobs.erase(job->Handle.Id);
    }

    m_JobCompleted.notify_all();
  }

  // NOTE: Cannot use profiling/logging - JobSystem is Layer 1, comes before
  // Profiler (Layer 2) and Logger (Layer 3)
}

std::shared_ptr<Job> ThreadPoolJobSystem::GetNextReadyJob() noexcept
{
  std::lock_guard<std::mutex> lock(m_Mutex);

  if (m_JobQueue.empty())
    return nullptr;

  // Find a job whose dependencies are complete
  std::priority_queue<std::shared_ptr<Job>, std::vector<std::shared_ptr<Job>>,
                      JobCompare>
      tempQueue;
  std::shared_ptr<Job> candidateJob = nullptr;

  while (!m_JobQueue.empty() && !candidateJob)
  {
    auto job = m_JobQueue.top();
    m_JobQueue.pop();

    if (AreJobDependenciesComplete(job))
    {
      candidateJob = job;
    }
    else
    {
      tempQueue.push(job);
    }
  }

  // Put back jobs that weren't ready
  while (!tempQueue.empty())
  {
    m_JobQueue.push(tempQueue.top());
    tempQueue.pop();
  }

  return candidateJob;
}

bool ThreadPoolJobSystem::AreJobDependenciesComplete(
    const std::shared_ptr<Job>& job) noexcept
{
  for (const auto& dependency : job->Dependencies)
  {
    auto it = m_ActiveJobs.find(dependency.Id);
    if (it != m_ActiveJobs.end() &&
        !it->second->Completed.load(std::memory_order_acquire))
    {
      return false;
    }
  }
  return true;
}

JobHandle ThreadPoolJobSystem::GenerateJobHandle() noexcept
{
  return JobHandle {m_NextJobId.fetch_add(1, std::memory_order_relaxed)};
}

}  // namespace gecko::runtime
