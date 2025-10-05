#include "gecko/runtime/thread_pool_job_system.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <thread>

#include "gecko/core/assert.h"
#include "gecko/core/log.h"
#include "gecko/core/profiler.h"
#include "gecko/core/time.h"

#include "categories.h"

namespace gecko::runtime {

bool ThreadPoolJobSystem::Init() noexcept {
  GECKO_ASSERT(!m_Initialized && "ThreadPoolJobSystem already initialized");

  // Determine worker thread count
  u32 workerCount = m_RequestedWorkerCount;
  if (workerCount == 0) {
    workerCount = std::max(1u, std::thread::hardware_concurrency());
  }

  // Note: Logger may not be available yet during JobSystem initialization
  // (JobSystem comes before Logger) Use direct fprintf for initialization
  // messages to avoid dependency issues
  std::fprintf(
      stderr,
      "[Gecko] Initializing ThreadPoolJobSystem with %u worker threads\n",
      workerCount);

  try {
    m_WorkerThreads.reserve(workerCount);
    for (u32 i = 0; i < workerCount; ++i) {
      m_WorkerThreads.emplace_back(&ThreadPoolJobSystem::WorkerThreadFunction,
                                   this);
    }

    m_Initialized = true;
    return true;
  } catch (...) {
    // Use direct fprintf during Init() since Logger may not be available yet
    std::fprintf(
        stderr,
        "[Gecko] Failed to create worker threads for ThreadPoolJobSystem\n");
    Shutdown();
    return false;
  }
}

void ThreadPoolJobSystem::Shutdown() noexcept {
  if (!m_Initialized)
    return;

  // Use direct fprintf during shutdown since logger will be shut down after job
  // system
  std::fprintf(stderr, "[Gecko] Shutting down ThreadPoolJobSystem\n");

  m_Shutdown.store(true, std::memory_order_release);
  m_JobAvailable.notify_all();

  for (auto &thread : m_WorkerThreads) {
    if (thread.joinable()) {
      thread.join();
    }
  }

  m_WorkerThreads.clear();

  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    // Clear remaining jobs
    while (!m_JobQueue.empty()) {
      m_JobQueue.pop();
    }
    m_ActiveJobs.clear();
  }

  m_Initialized = false;
}

JobHandle ThreadPoolJobSystem::Submit(JobFunction job, JobPriority priority,
                                      Category category) noexcept {
  if (!m_Initialized || !job)
    return JobHandle{};

  JobHandle handle = GenerateJobHandle();
  auto jobPtr =
      std::make_shared<Job>(std::move(job), priority, category, handle);

  {
    std::lock_guard<std::mutex> lock(m_Mutex);
    m_ActiveJobs[handle.Id] = jobPtr;
    m_JobQueue.push(jobPtr);
  }

  m_JobAvailable.notify_one();

  // Don't trace logger jobs to avoid feedback loops
  if (category != categories::Logger) {
  }
  return handle;
}

JobHandle ThreadPoolJobSystem::Submit(JobFunction job,
                                      const JobHandle *dependencies,
                                      u32 dependencyCount, JobPriority priority,
                                      Category category) noexcept {
  if (!m_Initialized || !job)
    return JobHandle{};

  JobHandle handle = GenerateJobHandle();
  auto jobPtr =
      std::make_shared<Job>(std::move(job), priority, category, handle);

  // Copy dependencies
  if (dependencies && dependencyCount > 0) {
    jobPtr->Dependencies.reserve(dependencyCount);
    for (u32 i = 0; i < dependencyCount; ++i) {
      if (dependencies[i].IsValid()) {
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

  // Don't trace logger jobs to avoid feedback loops
  if (category != categories::Logger) {
  }
  return handle;
}

void ThreadPoolJobSystem::Wait(JobHandle handle) noexcept {
  if (!handle.IsValid())
    return;

  GECKO_PROF_SCOPE(categories::Runtime, "ThreadPoolJobSystem::Wait");

  std::unique_lock<std::mutex> lock(m_Mutex);
  m_JobCompleted.wait(lock, [this, handle]() {
    auto it = m_ActiveJobs.find(handle.Id);
    return it == m_ActiveJobs.end() ||
           it->second->Completed.load(std::memory_order_acquire);
  });
}

void ThreadPoolJobSystem::WaitAll(const JobHandle *handles,
                                  u32 count) noexcept {
  if (!handles || count == 0)
    return;

  GECKO_PROF_SCOPE(categories::Runtime, "ThreadPoolJobSystem::WaitAll");

  std::unique_lock<std::mutex> lock(m_Mutex);
  m_JobCompleted.wait(lock, [this, handles, count]() {
    for (u32 i = 0; i < count; ++i) {
      if (!handles[i].IsValid())
        continue;

      auto it = m_ActiveJobs.find(handles[i].Id);
      if (it != m_ActiveJobs.end() &&
          !it->second->Completed.load(std::memory_order_acquire)) {
        return false;
      }
    }
    return true;
  });
}

bool ThreadPoolJobSystem::IsComplete(JobHandle handle) noexcept {
  if (!handle.IsValid())
    return true;

  std::lock_guard<std::mutex> lock(m_Mutex);
  auto it = m_ActiveJobs.find(handle.Id);
  return it == m_ActiveJobs.end() ||
         it->second->Completed.load(std::memory_order_acquire);
}

u32 ThreadPoolJobSystem::WorkerThreadCount() const noexcept {
  return static_cast<u32>(m_WorkerThreads.size());
}

void ThreadPoolJobSystem::ProcessJobs(u32 maxJobs) noexcept {
  GECKO_PROF_SCOPE(categories::Runtime, "ThreadPoolJobSystem::ProcessJobs");

  for (u32 processed = 0; processed < maxJobs; ++processed) {
    auto job = GetNextReadyJob();
    if (!job)
      break;

    try {
      GECKO_PROF_SCOPE(categories::Runtime, "Job::Execute");
      GECKO_PROF_COUNTER(categories::Runtime, "jobs_processed", 1);

      job->Function();
      job->Completed.store(true, std::memory_order_release);

    } catch (...) {
      job->Completed.store(true, std::memory_order_release);
    }

    m_JobCompleted.notify_all();
  }
}

void ThreadPoolJobSystem::WorkerThreadFunction() noexcept {
  GECKO_PROF_SCOPE(categories::Runtime, "WorkerThread");

  const u32 threadId = ThisThreadId();

  while (!m_Shutdown.load(std::memory_order_acquire)) {
    auto job = GetNextReadyJob();
    if (!job) {
      // No jobs available, wait for notification
      std::unique_lock<std::mutex> lock(m_Mutex);
      m_JobAvailable.wait_for(lock, std::chrono::milliseconds(100), [this]() {
        return m_Shutdown.load(std::memory_order_acquire) ||
               !m_JobQueue.empty();
      });
      continue;
    }

    try {
      GECKO_PROF_SCOPE(categories::Runtime, "Job::Execute");
      GECKO_PROF_COUNTER(categories::Runtime, "jobs_processed", 1);

      job->Function();
      job->Completed.store(true, std::memory_order_release);

    } catch (...) {
      GECKO_ERROR(categories::Runtime,
                  "Job %llu threw an exception on worker thread %u",
                  job->Handle.Id, threadId);
      job->Completed.store(true, std::memory_order_release);
    }

    m_JobCompleted.notify_all();
  }

  GECKO_TRACE(categories::Runtime, "Worker thread %u exiting", threadId);
}

std::shared_ptr<Job> ThreadPoolJobSystem::GetNextReadyJob() noexcept {
  std::lock_guard<std::mutex> lock(m_Mutex);

  if (m_JobQueue.empty())
    return nullptr;

  // Find a job whose dependencies are complete
  std::priority_queue<std::shared_ptr<Job>, std::vector<std::shared_ptr<Job>>,
                      JobCompare>
      tempQueue;
  std::shared_ptr<Job> candidateJob = nullptr;

  while (!m_JobQueue.empty() && !candidateJob) {
    auto job = m_JobQueue.top();
    m_JobQueue.pop();

    if (AreJobDependenciesComplete(job)) {
      candidateJob = job;
    } else {
      tempQueue.push(job);
    }
  }

  // Put back jobs that weren't ready
  while (!tempQueue.empty()) {
    m_JobQueue.push(tempQueue.top());
    tempQueue.pop();
  }

  return candidateJob;
}

bool ThreadPoolJobSystem::AreJobDependenciesComplete(
    const std::shared_ptr<Job> &job) noexcept {
  for (const auto &dependency : job->Dependencies) {
    auto it = m_ActiveJobs.find(dependency.Id);
    if (it != m_ActiveJobs.end() &&
        !it->second->Completed.load(std::memory_order_acquire)) {
      return false;
    }
  }
  return true;
}

JobHandle ThreadPoolJobSystem::GenerateJobHandle() noexcept {
  return JobHandle{m_NextJobId.fetch_add(1, std::memory_order_relaxed)};
}

} // namespace gecko::runtime
