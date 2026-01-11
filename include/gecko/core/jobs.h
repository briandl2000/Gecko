#pragma once

#include <functional>

#include "api.h"
#include "labels.h"
#include "types.h"

namespace gecko {

using JobFunction = std::function<void()>;

struct JobHandle {
  u64 Id{0};

  JobHandle() = default;
  explicit JobHandle(u64 id) noexcept : Id(id) {}

  bool IsValid() const noexcept { return Id != 0; }
  void Reset() noexcept { Id = 0; }

  bool operator==(const JobHandle &other) const noexcept {
    return Id == other.Id;
  }
  bool operator!=(const JobHandle &other) const noexcept {
    return Id != other.Id;
  }
};

enum class JobPriority : u8 { Low, Normal, High };

struct IJobSystem {
  GECKO_API virtual ~IJobSystem() = default;

  GECKO_API virtual JobHandle Submit(JobFunction job,
                                     JobPriority priority = JobPriority::Normal,
                                     Label label = Label{}) noexcept = 0;

  GECKO_API virtual JobHandle Submit(JobFunction job,
                                     const JobHandle *dependencies,
                                     u32 dependencyCount,
                                     JobPriority priority = JobPriority::Normal,
                                     Label label = Label{}) noexcept = 0;

  GECKO_API virtual void Wait(JobHandle handle) noexcept = 0;

  GECKO_API virtual void WaitAll(const JobHandle *handles,
                                 u32 count) noexcept = 0;

  GECKO_API virtual bool IsComplete(JobHandle handle) noexcept = 0;

  GECKO_API virtual u32 WorkerThreadCount() const noexcept = 0;

  GECKO_API virtual void ProcessJobs(u32 maxJobs = 1) noexcept = 0;

  GECKO_API virtual bool Init() noexcept = 0;
  GECKO_API virtual void Shutdown() noexcept = 0;
};

GECKO_API IJobSystem *GetJobSystem() noexcept;

GECKO_API inline JobHandle SubmitJob(JobFunction job,
                                     JobPriority priority = JobPriority::Normal,
                                     Label label = Label{}) noexcept {
  if (auto *jobSystem = GetJobSystem())
    return jobSystem->Submit(std::move(job), priority, label);
  return JobHandle{};
}

GECKO_API inline JobHandle SubmitJob(JobFunction job,
                                     const JobHandle *dependencies,
                                     u32 dependencyCount,
                                     JobPriority priority = JobPriority::Normal,
                                     Label label = Label{}) noexcept {
  if (auto *jobSystem = GetJobSystem())
    return jobSystem->Submit(std::move(job), dependencies, dependencyCount,
                             priority, label);
  return JobHandle{};
}

GECKO_API inline void WaitForJob(JobHandle handle) noexcept {
  if (auto *jobSystem = GetJobSystem())
    jobSystem->Wait(handle);
}

GECKO_API inline void WaitForJobs(const JobHandle *handles,
                                  u32 count) noexcept {
  if (auto *jobSystem = GetJobSystem())
    jobSystem->WaitAll(handles, count);
}

GECKO_API inline bool IsJobComplete(JobHandle handle) noexcept {
  if (auto *jobSystem = GetJobSystem())
    return jobSystem->IsComplete(handle);
  return true;
}

} // namespace gecko
