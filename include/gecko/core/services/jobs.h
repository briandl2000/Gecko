#pragma once

/// @file
/// Job system service interface for offloading work to worker threads.
///
/// Apps usually go through the free `SubmitJob` / `WaitForJob` /
/// `IsJobComplete` helpers, which forward to the active `IJobSystem`
/// or no-op when none is installed.

#include "gecko/core/api.h"
#include "gecko/core/labels.h"
#include "gecko/core/types.h"

#include <functional>

namespace gecko {

/// Opaque payload submitted to the job system.
///
/// Implementation note: this is currently `::std::function<void()>`, which
/// means the job system is **not** ABI-stable across compilers/STLs. It is
/// safe today because Gecko is one-toolchain-per-process; if cross-toolchain
/// plugins ever submit jobs, replace with a `(void (*)(void*), void*)` pair
/// or a small POD closure.
using JobFunction = ::std::function<void()>;  // abi-ok: see note above

/// Lightweight identifier for a submitted job. Cheap to copy. A value-
/// initialised handle (`Id == 0`) is the canonical "empty" handle and
/// satisfies `IsValid() == false`.
struct JobHandle
{
  u64 Id {0};  ///< Implementation-defined id; 0 means "none".

  JobHandle() = default;
  explicit JobHandle(u64 id) noexcept : Id(id)
  {}

  /// @return `true` if this handle refers to a submitted job.
  bool IsValid() const noexcept
  {
    return Id != 0;
  }
  /// Reset to the empty state.
  void Reset() noexcept
  {
    Id = 0;
  }

  bool operator==(const JobHandle& other) const noexcept
  {
    return Id == other.Id;
  }
  bool operator!=(const JobHandle& other) const noexcept
  {
    return Id != other.Id;
  }
};

/// Dispatch priority hint for the scheduler.
enum class JobPriority : u8
{
  Low,     ///< Background work; only main thread will run it via `ProcessJobs`.
  Normal,  ///< Default worker priority.
  High     ///< Latency-sensitive work; scheduled ahead of Normal.
};

/// Job system service interface.
struct IJobSystem
{
  GECKO_API virtual ~IJobSystem() = default;

  /// Submit a job for asynchronous execution.
  /// @param job Function object to invoke on a worker thread.
  /// @param priority Scheduler hint.
  /// @param label Memory label active during the job.
  /// @return A handle usable with `Wait` / `IsComplete`.
  GECKO_API virtual JobHandle Submit(JobFunction job,
                                     JobPriority priority = JobPriority::Normal,
                                     Label label = {}) noexcept = 0;

  /// Submit a job that runs only after every dependency has finished.
  /// @param job Function object to invoke.
  /// @param dependencies Pointer to an array of handles.
  /// @param dependencyCount Number of entries in `dependencies`.
  /// @param priority Scheduler hint.
  /// @param label Memory label active during the job.
  GECKO_API virtual JobHandle Submit(JobFunction job,
                                     const JobHandle* dependencies,
                                     u32 dependencyCount,
                                     JobPriority priority = JobPriority::Normal,
                                     Label label = {}) noexcept = 0;

  /// Block the calling thread until a job completes. Safe to call
  /// with an invalid handle.
  /// @param handle Job to wait on.
  GECKO_API virtual void Wait(JobHandle handle) noexcept = 0;

  /// Block until every supplied handle has completed.
  /// @param handles Pointer to an array of handles.
  /// @param count Number of entries in `handles`.
  GECKO_API virtual void WaitAll(const JobHandle* handles,
                                 u32 count) noexcept = 0;

  /// @param handle Job to query.
  /// @return `true` if `handle` has finished or was never valid.
  GECKO_API virtual bool IsComplete(JobHandle handle) noexcept = 0;

  /// @return Number of worker threads owned by this scheduler. May be
  ///         zero (single-threaded fallback).
  GECKO_API virtual u32 WorkerThreadCount() const noexcept = 0;

  /// Drain queued `Low`-priority jobs on the calling thread. Use this
  /// to flush deferred work from the main loop.
  /// @param maxJobs Maximum number of jobs to process this call.
  GECKO_API virtual void ProcessJobs(u32 maxJobs = 1) noexcept = 0;

  /// One-time setup; called when the job system is installed.
  GECKO_API virtual bool Init() noexcept = 0;
  /// Counterpart to `Init`; waits for in-flight jobs to drain.
  GECKO_API virtual void Shutdown() noexcept = 0;
};

/// @return Active job system, or `NullJobSystem` if none is installed.
GECKO_API IJobSystem* GetJobSystem() noexcept;

/// Convenience wrapper around `IJobSystem::Submit`.
inline JobHandle SubmitJob(JobFunction job,
                           JobPriority priority = JobPriority::Normal,
                           Label label = {}) noexcept
{
  // abi-ok: inline helper, instantiated per TU; does not cross the boundary.
  auto* jobSystem = GetJobSystem();
  return jobSystem ? jobSystem->Submit(::std::move(job), priority, label)
                   : JobHandle {};
}

/// Convenience wrapper that submits a job with explicit dependencies.
inline JobHandle SubmitJob(JobFunction job, const JobHandle* dependencies,
                           u32 dependencyCount,
                           JobPriority priority = JobPriority::Normal,
                           Label label = {}) noexcept
{
  // abi-ok: inline helper, instantiated per TU; does not cross the boundary.
  auto* jobSystem = GetJobSystem();
  if (!jobSystem)
    return JobHandle {};
  return jobSystem->Submit(::std::move(job), dependencies, dependencyCount,
                           priority, label);
}

/// Block until the supplied handle completes.
/// @param handle Job to wait on.
inline void WaitForJob(JobHandle handle) noexcept
{
  if (auto* jobSystem = GetJobSystem())
    jobSystem->Wait(handle);
}

/// Block until every entry in `handles` completes.
/// @param handles Pointer to an array of handles.
/// @param count Number of entries in `handles`.
inline void WaitForJobs(const JobHandle* handles, u32 count) noexcept
{
  if (auto* jobSystem = GetJobSystem())
    jobSystem->WaitAll(handles, count);
}

/// @param handle Job to query.
/// @return `true` if the job has completed (or no job system is
///         installed).
inline bool IsJobComplete(JobHandle handle) noexcept
{
  if (auto* jobSystem = GetJobSystem())
    return jobSystem->IsComplete(handle);
  return true;
}

struct NullJobSystem final : IJobSystem
{
  GECKO_API virtual JobHandle Submit(JobFunction job,
                                     JobPriority priority = JobPriority::Normal,
                                     Label label = Label {}) noexcept override;
  GECKO_API virtual JobHandle Submit(JobFunction job,
                                     const JobHandle* dependencies,
                                     u32 dependencyCount,
                                     JobPriority priority = JobPriority::Normal,
                                     Label label = Label {}) noexcept override;
  GECKO_API virtual void Wait(JobHandle handle) noexcept override;
  GECKO_API virtual void WaitAll(const JobHandle* handles,
                                 u32 count) noexcept override;
  GECKO_API virtual bool IsComplete(JobHandle handle) noexcept override;
  GECKO_API virtual u32 WorkerThreadCount() const noexcept override;
  GECKO_API virtual void ProcessJobs(u32 maxJobs = 1) noexcept override;

  GECKO_API virtual bool Init() noexcept override;
  GECKO_API virtual void Shutdown() noexcept override;
};

}  // namespace gecko
