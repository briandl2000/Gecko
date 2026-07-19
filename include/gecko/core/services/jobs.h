#pragma once

#include "gecko/api.h"
#include "gecko/core/labels.h"
#include "gecko/core/placement.h"
#include "gecko/core/services/memory.h"
#include "gecko/core/types.h"
#include "gecko/core/utility/move.h"

namespace gecko {

struct JobFn
{
  void (*Invoke)(void* user) {nullptr};
  void (*Free)(void* user) noexcept {nullptr};
  void* User {nullptr};

  [[nodiscard]] bool IsValid() const noexcept
  {
    return Invoke != nullptr;
  }
};

struct JobHandle
{
  u64 Id {0};

  [[nodiscard]] bool IsValid() const noexcept
  {
    return Id != 0;
  }
  void Reset() noexcept
  {
    Id = 0;
  }
  bool operator==(JobHandle other) const noexcept
  {
    return Id == other.Id;
  }
};

enum class JobPriority : u8
{
  Low,
  Normal,
  High,
};

[[nodiscard]] GECKO_API JobHandle SubmitRawJob(JobFn job, JobPriority priority = JobPriority::Normal,
                                               Label label = {}) noexcept;
[[nodiscard]] GECKO_API JobHandle SubmitRawJob(JobFn job, const JobHandle* dependencies, u32 dependencyCount,
                                               JobPriority priority = JobPriority::Normal, Label label = {}) noexcept;
GECKO_API void WaitForJob(JobHandle handle) noexcept;
GECKO_API void WaitForJobs(const JobHandle* handles, u32 count) noexcept;
[[nodiscard]] GECKO_API bool IsJobComplete(JobHandle handle) noexcept;
[[nodiscard]] GECKO_API u32 JobWorkerCount() noexcept;
GECKO_API void ProcessJobs(u32 maxJobs = 1) noexcept;

namespace detail {

template <class F>
JobFn MakeJobFn(F&& function) noexcept
{
  using Fn = RemoveCVRef<F>;
  void* storage = AllocBytes(sizeof(Fn), alignof(Fn));
  if (storage == nullptr)
    return {};
  auto* state = new (storage, Placement) Fn(Forward<F>(function));
  return JobFn {
      .Invoke = [](void* user) { (*static_cast<Fn*>(user))(); },
      .Free =
          [](void* user) noexcept {
            auto* value = static_cast<Fn*>(user);
            value->~Fn();
            DeallocBytes(value);
          },
      .User = state,
  };
}

}  // namespace detail

template <class F>
[[nodiscard]] JobHandle SubmitJob(F&& function, JobPriority priority = JobPriority::Normal, Label label = {}) noexcept
{
  return SubmitRawJob(detail::MakeJobFn(Forward<F>(function)), priority, label);
}

template <class F>
[[nodiscard]] JobHandle SubmitJob(F&& function, const JobHandle* dependencies, u32 dependencyCount,
                                  JobPriority priority = JobPriority::Normal, Label label = {}) noexcept
{
  return SubmitRawJob(detail::MakeJobFn(Forward<F>(function)), dependencies, dependencyCount, priority, label);
}

}  // namespace gecko
