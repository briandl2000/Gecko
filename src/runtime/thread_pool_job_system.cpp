#include "gecko/runtime/thread_pool_job_system.h"

#include "gecko/core/assert.h"
#include "gecko/core/scope.h"
#include "gecko/core/services/log.h"
#include "gecko/core/services/profiler.h"
#include "private/labels.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <functional>
#include <memory>
#include <mutex>
#include <new>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>

namespace gecko::runtime {

struct ThreadPoolJobSystem::Impl
{
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
      if (Function.Free && Function.User)
        Function.Free(Function.User);
    }

    Job(const Job&) = delete;
    Job& operator=(const Job&) = delete;
    Job(Job&&) = delete;
    Job& operator=(Job&&) = delete;
  };

  struct JobCompare
  {
    bool operator()(const std::shared_ptr<Job>& a, const std::shared_ptr<Job>& b) const
    {
      return static_cast<int>(a->Priority) < static_cast<int>(b->Priority);
    }
  };

  mutable std::mutex Mutex;
  std::condition_variable JobAvailable;
  std::condition_variable JobCompleted;

  std::priority_queue<std::shared_ptr<Job>, std::vector<std::shared_ptr<Job>>, JobCompare> JobQueue;
  std::unordered_map<u64, std::shared_ptr<Job>> ActiveJobs;

  std::vector<std::thread> WorkerThreads;
  std::atomic<bool> Shutdown {false};
  std::atomic<u64> NextJobId {1};

  u32 RequestedWorkerCount {0};
  bool Initialized {false};
};

template <class ImplT>
bool AreJobDependenciesComplete(ImplT& impl, const std::shared_ptr<typename ImplT::Job>& job) noexcept
{
  for (const auto& dependency : job->Dependencies)
  {
    auto it = impl.ActiveJobs.find(dependency.Id);
    if (it != impl.ActiveJobs.end() && !it->second->Completed.load(std::memory_order_acquire))
    {
      return false;
    }
  }
  return true;
}

template <class ImplT>
auto GetNextReadyJob(ImplT& impl) noexcept
{
  std::lock_guard<std::mutex> lock(impl.Mutex);

  if (impl.JobQueue.empty())
    return std::shared_ptr<typename ImplT::Job> {};

  std::priority_queue<std::shared_ptr<typename ImplT::Job>, std::vector<std::shared_ptr<typename ImplT::Job>>,
                      typename ImplT::JobCompare>
      tempQueue;
  std::shared_ptr<typename ImplT::Job> candidateJob = nullptr;

  while (!impl.JobQueue.empty() && !candidateJob)
  {
    auto job = impl.JobQueue.top();
    impl.JobQueue.pop();

    if (AreJobDependenciesComplete(impl, job))
      candidateJob = job;
    else
      tempQueue.push(job);
  }

  while (!tempQueue.empty())
  {
    impl.JobQueue.push(tempQueue.top());
    tempQueue.pop();
  }

  return candidateJob;
}

ThreadPoolJobSystem::ThreadPoolJobSystem() noexcept : m_Impl(new (::std::nothrow) Impl())
{}

ThreadPoolJobSystem::~ThreadPoolJobSystem()
{
  Shutdown();
}

bool ThreadPoolJobSystem::Init() noexcept
{
  // NOTE: Cannot profile Init() - profiler is initialized AFTER job system
  if (!m_Impl)
    return false;

  GECKO_ASSERT(!m_Impl->Initialized && "ThreadPoolJobSystem already initialized");

  // Determine worker thread count
  u32 workerCount = m_Impl->RequestedWorkerCount;
  if (workerCount == 0)
  {
    workerCount = std::max(1u, std::thread::hardware_concurrency());
  }

  try
  {
    m_Impl->WorkerThreads.reserve(workerCount);
    for (u32 i = 0; i < workerCount; ++i)
    {
      m_Impl->WorkerThreads.emplace_back(&ThreadPoolJobSystem::WorkerThreadFunction, this, i);
    }

    m_Impl->Initialized = true;
    return true;
  }
  catch (...)
  {
    // Use direct fprintf during Init() since Logger may not be available yet
    std::fprintf(stderr, "[Gecko] Failed to create worker threads for ThreadPoolJobSystem\n");
    Shutdown();
    return false;
  }
}

void ThreadPoolJobSystem::Shutdown() noexcept
{
  if (!m_Impl || !m_Impl->Initialized)
    return;

  m_Impl->Shutdown.store(true, std::memory_order_release);
  m_Impl->JobAvailable.notify_all();

  for (auto& thread : m_Impl->WorkerThreads)
  {
    if (thread.joinable())
      thread.join();
  }

  // Force complete deallocation before allocator shutdown
  decltype(m_Impl->WorkerThreads)().swap(m_Impl->WorkerThreads);

  {
    std::lock_guard<std::mutex> lock(m_Impl->Mutex);
    decltype(m_Impl->JobQueue)().swap(m_Impl->JobQueue);
    decltype(m_Impl->ActiveJobs)().swap(m_Impl->ActiveJobs);
  }

  m_Impl->Initialized = false;
}

JobHandle ThreadPoolJobSystem::SubmitRaw(JobFn job, JobPriority priority, Label label) noexcept
{
  // Profiler/logger may not exist yet at very early startup; the macros
  // route through GetProfiler()/GetLogger() which fall back to Null impls.
  GECKO_PROFILE_NAMED(labels::JobSystem, "JobSystem::Submit");

  if (!m_Impl || !m_Impl->Initialized || !job.IsValid())
  {
    if (job.Free && job.User)
      job.Free(job.User);
    return JobHandle {};
  }

  JobHandle handle = GenerateJobHandle();
  auto jobPtr = std::make_shared<Impl::Job>(job, priority, label, handle);

  {
    std::lock_guard<std::mutex> lock(m_Impl->Mutex);
    m_Impl->ActiveJobs[handle.Id] = jobPtr;
    m_Impl->JobQueue.push(jobPtr);
  }

  m_Impl->JobAvailable.notify_one();
  return handle;
}

JobHandle ThreadPoolJobSystem::SubmitRaw(JobFn job, const JobHandle* dependencies, u32 dependencyCount,
                                         JobPriority priority, Label label) noexcept
{
  GECKO_PROFILE_NAMED(labels::JobSystem, "JobSystem::Submit(deps)");

  if (!m_Impl || !m_Impl->Initialized || !job.IsValid())
  {
    if (job.Free && job.User)
      job.Free(job.User);
    return JobHandle {};
  }

  JobHandle handle = GenerateJobHandle();
  auto jobPtr = std::make_shared<Impl::Job>(job, priority, label, handle);

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
    std::lock_guard<std::mutex> lock(m_Impl->Mutex);
    m_Impl->ActiveJobs[handle.Id] = jobPtr;
    m_Impl->JobQueue.push(jobPtr);
  }

  m_Impl->JobAvailable.notify_one();
  return handle;
}

void ThreadPoolJobSystem::Wait(JobHandle handle) noexcept
{
  if (!m_Impl || !handle.IsValid())
    return;

  GECKO_PROFILE_NAMED(labels::JobSystem, "JobSystem::Wait");
  std::unique_lock<std::mutex> lock(m_Impl->Mutex);
  m_Impl->JobCompleted.wait(lock, [this, handle]() {
    auto it = m_Impl->ActiveJobs.find(handle.Id);
    return it == m_Impl->ActiveJobs.end() || it->second->Completed.load(std::memory_order_acquire);
  });
}

void ThreadPoolJobSystem::WaitAll(const JobHandle* handles, u32 count) noexcept
{
  if (!m_Impl || !handles || count == 0)
    return;

  GECKO_PROFILE_NAMED(labels::JobSystem, "JobSystem::WaitAll");
  std::unique_lock<std::mutex> lock(m_Impl->Mutex);
  m_Impl->JobCompleted.wait(lock, [this, handles, count]() {
    for (u32 i = 0; i < count; ++i)
    {
      if (!handles[i].IsValid())
        continue;

      auto it = m_Impl->ActiveJobs.find(handles[i].Id);
      if (it != m_Impl->ActiveJobs.end() && !it->second->Completed.load(std::memory_order_acquire))
      {
        return false;
      }
    }
    return true;
  });
}

bool ThreadPoolJobSystem::IsComplete(JobHandle handle) noexcept
{
  if (!m_Impl || !handle.IsValid())
    return true;

  // Fast path: try to find the job and check completion without holding
  // the lock for the entire duration. We use a lock to safely get the
  // shared_ptr, then check the atomic outside the lock.
  std::shared_ptr<Impl::Job> job;
  {
    std::lock_guard<std::mutex> lock(m_Impl->Mutex);
    auto it = m_Impl->ActiveJobs.find(handle.Id);
    if (it == m_Impl->ActiveJobs.end())
      return true;  // Job not found = completed and cleaned up
    job = it->second;
  }

  // Check completion flag outside the lock
  return job->Completed.load(std::memory_order_acquire);
}

u32 ThreadPoolJobSystem::WorkerThreadCount() const noexcept
{
  return m_Impl ? static_cast<u32>(m_Impl->WorkerThreads.size()) : 0;
}

void ThreadPoolJobSystem::ProcessJobs(u32 maxJobs) noexcept
{
  for (u32 processed = 0; processed < maxJobs; ++processed)
  {
    if (!m_Impl)
      return;

    auto job = GetNextReadyJob(*m_Impl);
    if (!job)
      break;

    try
    {
      if (job->Function.Invoke)
        job->Function.Invoke(job->Function.User);
      if (job->Function.Free && job->Function.User)
        job->Function.Free(job->Function.User);
      job->Function = JobFn {};
      job->Completed.store(true, std::memory_order_release);
    }
    catch (...)
    {
      if (job->Function.Free && job->Function.User)
        job->Function.Free(job->Function.User);
      job->Function = JobFn {};
      job->Completed.store(true, std::memory_order_release);
    }

    {
      std::lock_guard<std::mutex> lock(m_Impl->Mutex);
      m_Impl->ActiveJobs.erase(job->Handle.Id);
    }

    m_Impl->JobCompleted.notify_all();
  }
}

void ThreadPoolJobSystem::WorkerThreadFunction(u32 workerIndex) noexcept
{
  // NOTE: Cannot use profiling/logging - JobSystem is Layer 1, comes before
  // Profiler (Layer 2) and Logger (Layer 3)

  // Profiler thread-name registration is layer-independent (it just stores a
  // pointer in a process-global table) and lets trace sinks emit
  // chrome-trace `thread_name` records for these workers.
  static constexpr const char* WorkerNames[] = {
      "job-worker-0",  "job-worker-1",  "job-worker-2",  "job-worker-3",  "job-worker-4",  "job-worker-5",
      "job-worker-6",  "job-worker-7",  "job-worker-8",  "job-worker-9",  "job-worker-10", "job-worker-11",
      "job-worker-12", "job-worker-13", "job-worker-14", "job-worker-15",
  };
  const char* name =
      (workerIndex < (sizeof(WorkerNames) / sizeof(WorkerNames[0]))) ? WorkerNames[workerIndex] : "job-worker-N";
  ::gecko::SetThreadProfilerName(name);

  if (!m_Impl)
    return;

  while (!m_Impl->Shutdown.load(std::memory_order_acquire))
  {
    auto job = GetNextReadyJob(*m_Impl);
    if (!job)
    {
      // No jobs available, wait for notification
      GECKO_PROFILE_NAMED(labels::JobSystem, "JobSystem::WorkerIdle");
      std::unique_lock<std::mutex> lock(m_Impl->Mutex);
      m_Impl->JobAvailable.wait_for(lock, std::chrono::milliseconds(100), [this]() {
        return m_Impl->Shutdown.load(std::memory_order_acquire) || !m_Impl->JobQueue.empty();
      });
      continue;
    }

    try
    {
      GECKO_PROFILE_NAMED(labels::JobSystem, "JobSystem::Run");
      if (job->Function.Invoke)
        job->Function.Invoke(job->Function.User);
      if (job->Function.Free && job->Function.User)
        job->Function.Free(job->Function.User);
      job->Function = JobFn {};
      job->Completed.store(true, std::memory_order_release);
    }
    catch (...)
    {
      if (job->Function.Free && job->Function.User)
        job->Function.Free(job->Function.User);
      job->Function = JobFn {};
      job->Completed.store(true, std::memory_order_release);
    }

    {
      std::lock_guard<std::mutex> lock(m_Impl->Mutex);
      m_Impl->ActiveJobs.erase(job->Handle.Id);
    }

    m_Impl->JobCompleted.notify_all();
  }

  // NOTE: Cannot use profiling/logging - JobSystem is Layer 1, comes before
  // Profiler (Layer 2) and Logger (Layer 3)
}

JobHandle ThreadPoolJobSystem::GenerateJobHandle() noexcept
{
  return m_Impl ? JobHandle {m_Impl->NextJobId.fetch_add(1, std::memory_order_relaxed)} : JobHandle {};
}

void ThreadPoolJobSystem::SetWorkerThreadCount(u32 count) noexcept
{
  if (m_Impl)
    m_Impl->RequestedWorkerCount = count;
}

}  // namespace gecko::runtime
