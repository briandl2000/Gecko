#pragma once

/// @file
/// Job system service interface for offloading work to worker threads.
///
/// Apps usually go through the free `SubmitJob` / `WaitForJob` /
/// `IsJobComplete` helpers, or call `IJobSystem::Submit(callable, ...)`
/// directly. Both forward to the `SubmitRaw(JobFn, ...)` virtual after
/// boxing the callable into an ABI-stable `JobFn` payload.

#include "gecko/core/api.h"
#include "gecko/core/labels.h"
#include "gecko/core/types.h"

#include <new>
#include <type_traits>
#include <utility>

namespace gecko {

/// ABI-stable job payload. Carries native function pointers only, so it
/// is safe to cross the CoreServices DLL boundary regardless of which
/// toolchain produced the closure.
///
/// `Invoke` runs the work on a worker thread. `Free` releases captured
/// state once the job has finished (or is cancelled before running);
/// pass `nullptr` if `User` has static lifetime. `User` is opaque to
/// the job system.
struct JobFn
{
  void (*Invoke)(void* user) {nullptr};
  void (*Free)(void* user) noexcept {nullptr};
  void* User {nullptr};

  /// @return `true` if this payload has an invoke function.
  bool IsValid() const noexcept
  {
    return Invoke != nullptr;
  }
};

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

namespace detail {

// abi-ok-begin: header-only template, instantiated per consumer TU. The
// closure is allocated and freed by the same toolchain that produced it,
// so std:: type uses below never appear in CoreServices.dll itself.

/// Boxes any callable into an ABI-stable `JobFn`. Stateful callables
/// are heap-allocated in the caller's TU so allocation, invocation, and
/// deallocation all use the same toolchain's runtime. Stateless
/// callables that decay to a function pointer are also heap-boxed: a
/// raw `void(*)()` cannot be portably round-tripped through `void*`
/// (object<->function pointer conversions are UB in standard C++), and
/// the alternative -- adding a function-pointer slot to `JobFn` -- is
/// not worth the ABI churn for the few stateless cases we have.
///
/// Returns an empty (`!IsValid()`) `JobFn` on allocation failure so
/// callers can keep `noexcept` semantics.
template <class F>
inline JobFn MakeJobFn(F&& f) noexcept
{
  using Fn = ::std::decay_t<F>;
  Fn* state = nullptr;
  try
  {
    state = new (::std::nothrow) Fn(::std::forward<F>(f));
  }
  catch (...)
  {
    // Fn's copy/move constructor threw; treat like an allocation
    // failure so callers see an empty JobFn rather than terminate.
    return JobFn {};
  }
  if (!state)
    return JobFn {};
  JobFn job {};
  job.Invoke = [](void* u) { (*static_cast<Fn*>(u))(); };
  job.Free = [](void* u) noexcept { delete static_cast<Fn*>(u); };
  job.User = state;
  return job;
}

// abi-ok-end

}  // namespace detail

/// Job system service interface.
struct IJobSystem
{
  GECKO_API virtual ~IJobSystem() = default;

  /// Submit a pre-built `JobFn` for asynchronous execution.
  /// @param job ABI-stable payload; see `JobFn`.
  /// @param priority Scheduler hint.
  /// @param label Memory label active during the job.
  /// @return A handle usable with `Wait` / `IsComplete`.
  GECKO_API virtual JobHandle SubmitRaw(JobFn job, JobPriority priority = JobPriority::Normal,
                                        Label label = {}) noexcept = 0;

  /// Submit a pre-built `JobFn` that runs only after every dependency has
  /// finished.
  /// @param job ABI-stable payload; see `JobFn`.
  /// @param dependencies Pointer to an array of handles.
  /// @param dependencyCount Number of entries in `dependencies`.
  /// @param priority Scheduler hint.
  /// @param label Memory label active during the job.
  GECKO_API virtual JobHandle SubmitRaw(JobFn job, const JobHandle* dependencies, u32 dependencyCount,
                                        JobPriority priority = JobPriority::Normal, Label label = {}) noexcept = 0;

  /// Convenience overload that boxes any callable (lambda, function
  /// pointer, `std::function`, ...) into a `JobFn` and forwards to
  /// `SubmitRaw`. Boxing happens in the caller's TU, so the closure's
  /// memory is allocated and freed by the same toolchain that produced
  /// it -- safe to call across the CoreServices DLL boundary.
  // abi-ok-begin: header-only templates; std:: forwarding refs are
  // resolved per consumer TU and never reach the dispatched virtual.
  template <class F>
  JobHandle Submit(F&& f, JobPriority priority = JobPriority::Normal, Label label = {}) noexcept
  {
    JobFn job = detail::MakeJobFn(::std::forward<F>(f));
    if (!job.IsValid())
      return JobHandle {};
    return SubmitRaw(job, priority, label);
  }

  /// Convenience overload with explicit dependencies; see the unary
  /// `Submit` for the boxing rules.
  template <class F>
  JobHandle Submit(F&& f, const JobHandle* dependencies, u32 dependencyCount,
                   JobPriority priority = JobPriority::Normal, Label label = {}) noexcept
  {
    JobFn job = detail::MakeJobFn(::std::forward<F>(f));
    if (!job.IsValid())
      return JobHandle {};
    return SubmitRaw(job, dependencies, dependencyCount, priority, label);
  }
  // abi-ok-end

  /// Block the calling thread until a job completes. Safe to call
  /// with an invalid handle.
  /// @param handle Job to wait on.
  GECKO_API virtual void Wait(JobHandle handle) noexcept = 0;

  /// Block until every supplied handle has completed.
  /// @param handles Pointer to an array of handles.
  /// @param count Number of entries in `handles`.
  GECKO_API virtual void WaitAll(const JobHandle* handles, u32 count) noexcept = 0;

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

// abi-ok-begin: free helpers are header-only templates; same per-TU
// argument as IJobSystem::Submit.

/// Convenience wrapper around `IJobSystem::Submit`.
template <class F>
inline JobHandle SubmitJob(F&& f, JobPriority priority = JobPriority::Normal, Label label = {}) noexcept
{
  auto* jobSystem = GetJobSystem();
  return jobSystem ? jobSystem->Submit(::std::forward<F>(f), priority, label) : JobHandle {};
}

/// Convenience wrapper that submits a job with explicit dependencies.
template <class F>
inline JobHandle SubmitJob(F&& f, const JobHandle* dependencies, u32 dependencyCount,
                           JobPriority priority = JobPriority::Normal, Label label = {}) noexcept
{
  auto* jobSystem = GetJobSystem();
  if (!jobSystem)
    return JobHandle {};
  return jobSystem->Submit(::std::forward<F>(f), dependencies, dependencyCount, priority, label);
}

// abi-ok-end

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
  GECKO_API virtual JobHandle SubmitRaw(JobFn job, JobPriority priority = JobPriority::Normal,
                                        Label label = Label {}) noexcept override;
  GECKO_API virtual JobHandle SubmitRaw(JobFn job, const JobHandle* dependencies, u32 dependencyCount,
                                        JobPriority priority = JobPriority::Normal,
                                        Label label = Label {}) noexcept override;
  GECKO_API virtual void Wait(JobHandle handle) noexcept override;
  GECKO_API virtual void WaitAll(const JobHandle* handles, u32 count) noexcept override;
  GECKO_API virtual bool IsComplete(JobHandle handle) noexcept override;
  GECKO_API virtual u32 WorkerThreadCount() const noexcept override;
  GECKO_API virtual void ProcessJobs(u32 maxJobs = 1) noexcept override;

  GECKO_API virtual bool Init() noexcept override;
  GECKO_API virtual void Shutdown() noexcept override;
};

}  // namespace gecko
