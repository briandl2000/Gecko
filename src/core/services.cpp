#include "gecko/core/services/profiler.h"
#include "gecko/core/containers/string.h"
#include "gecko/core/sync.h"

namespace gecko {

namespace {

struct ThreadNameEntry
{
  StaticString<63> Name;
  u32 ThreadId {0};
  bool Active {false};
};

ThreadNameEntry g_ThreadNames[64] {};
SpinMutex g_ThreadNameMutex;
thread_local StaticString<63> g_CurrentThreadName;

}  // namespace

void SetThreadProfilerName(const char* name) noexcept
{
  g_CurrentThreadName.Clear();
  if (name != nullptr)
    g_CurrentThreadName.Append(StringView {name}.Substring(0, 63));
  RegisterThreadProfilerName(ThisThreadId(), name);
}

const char* GetThreadProfilerName() noexcept
{
  return g_CurrentThreadName.Count() != 0 ? g_CurrentThreadName.Data() : nullptr;
}

const char* LookupThreadProfilerName(u32 threadId) noexcept
{
  LockGuard lock(g_ThreadNameMutex);
  const char* result = nullptr;
  for (const ThreadNameEntry& entry : g_ThreadNames)
  {
    if (entry.Active && entry.ThreadId == threadId)
    {
      result = entry.Name.Data();
      break;
    }
  }
  return result;
}

void RegisterThreadProfilerName(u32 threadId, const char* name) noexcept
{
  LockGuard lock(g_ThreadNameMutex);
  ThreadNameEntry* destination = nullptr;
  for (ThreadNameEntry& entry : g_ThreadNames)
  {
    if (entry.Active && entry.ThreadId == threadId)
    {
      destination = &entry;
      break;
    }
    if (!entry.Active && destination == nullptr)
      destination = &entry;
  }
  if (destination != nullptr)
  {
    destination->Name.Clear();
    destination->ThreadId = threadId;
    destination->Active = name != nullptr;
    if (name != nullptr)
      destination->Name.Append(StringView {name}.Substring(0, 63));
  }
}

}  // namespace gecko
