#include "gecko/runtime/tracking_allocator.h"

#include "gecko/core/assert.h"
#include "gecko/core/services/log.h"
#include "gecko/core/services/profiler.h"
#include "private/labels.h"

#include <array>
#include <atomic>
#include <cstdlib>
#include <functional>
#include <mutex>
#include <new>
#include <unordered_map>
#include <utility>

namespace gecko::runtime {

namespace {

template <typename T>
class MallocAllocator
{
  static_assert(alignof(T) <= alignof(::std::max_align_t), "MallocAllocator does not support over-aligned types");

public:
  using value_type = T;

  MallocAllocator() noexcept = default;

  template <typename U>
  MallocAllocator(const MallocAllocator<U>&) noexcept
  {}

  T* allocate(::std::size_t n)
  {
    void* ptr = ::std::malloc(n * sizeof(T));
    if (!ptr)
      throw ::std::bad_alloc {};
    return static_cast<T*>(ptr);
  }

  void deallocate(T* ptr, ::std::size_t) noexcept
  {
    ::std::free(ptr);
  }

  template <typename U>
  bool operator==(const MallocAllocator<U>&) const noexcept
  {
    return true;
  }

  template <typename U>
  bool operator!=(const MallocAllocator<U>&) const noexcept
  {
    return false;
  }
};

}  // namespace

struct TrackingAllocator::Impl
{
  mutable std::mutex Mutex;

  std::unordered_map<u64, MemLabelStats, std::hash<u64>, std::equal_to<u64>,
                     MallocAllocator<std::pair<const u64, MemLabelStats>>>
      ByLabel;

  std::atomic<u64> TotalLive {0};
  IProfiler* Profiler {nullptr};
};

// ------------------------------------------------------------
// Thread-local allocation context
// ------------------------------------------------------------

namespace {
constexpr Label g_DefaultLabel = MakeLabel("gecko.default");

struct ThreadAllocContext
{
  std::array<Label, MaxLabelStackDepth> LabelStack {};
  u32 StackDepth {0};
};

thread_local ThreadAllocContext g_AllocContext;

}  // namespace

TrackingAllocator::TrackingAllocator() noexcept : m_Impl(new (::std::nothrow) Impl())
{}

TrackingAllocator::~TrackingAllocator() noexcept = default;

// ------------------------------------------------------------
// Label Stack
// ------------------------------------------------------------

void TrackingAllocator::PushLabel(Label label) noexcept
{
  if (g_AllocContext.StackDepth < MaxLabelStackDepth)
  {
    g_AllocContext.LabelStack[g_AllocContext.StackDepth] = label;
    ++g_AllocContext.StackDepth;
  }
}

void TrackingAllocator::PopLabel() noexcept
{
  if (g_AllocContext.StackDepth > 0)
  {
    --g_AllocContext.StackDepth;
  }
}

Label TrackingAllocator::CurrentLabel() const noexcept
{
  if (g_AllocContext.StackDepth == 0)
  {
    return g_DefaultLabel;
  }
  return g_AllocContext.LabelStack[g_AllocContext.StackDepth - 1];
}

// ------------------------------------------------------------
// Alloc / Free
// ------------------------------------------------------------

template <class ImplT>
static MemLabelStats& EnsureLabelLocked(ImplT& impl, Label label)
{
  auto result = impl.ByLabel.try_emplace(label.Id);
  auto& stats = result.first->second;
  if (result.second)
  {
    stats.StatsLabel = label;
  }
  return stats;
}

void* TrackingAllocator::Alloc(u64 size, u32 alignment) noexcept
{
  if (!m_Impl)
    return nullptr;

  GECKO_ASSERT(size > 0 && "Cannot allocate zero bytes");
  GECKO_ASSERT(alignment > 0 && (alignment & (alignment - 1)) == 0 && "Alignment must be power of 2");

  const Label label = CurrentLabel();
  const u32 effAlign = EffectiveAlignment(alignment);
  const u64 totalSize = TotalAllocSize(size, alignment);

  void* rawPtr = PlatformAlloc(totalSize, effAlign);
  if (!rawPtr)
    return nullptr;

  void* userPtr = PlaceAllocHeader(rawPtr, size, alignment, TrackingAllocMagic, label, this);

  m_Impl->TotalLive.fetch_add(size, std::memory_order_relaxed);

  {
    std::lock_guard<std::mutex> lk(m_Impl->Mutex);
    auto& st = EnsureLabelLocked(*m_Impl, label);
    st.LiveBytes += size;
    st.Allocs += 1;
  }

  return userPtr;
}

void TrackingAllocator::Free(void* ptr) noexcept
{
  if (!ptr)
    return;

  auto* header = HeaderFromUserPtr(ptr);

  if (header->Magic == TrackingAllocMagic)
  {
    const u64 size = header->RequestedSize;
    const Label label = header->AllocLabel;

    void* rawPtr = RawPtrFromHeader(header);
    u32 alignment = header->Alignment;
    header->Magic = 0;  // Poison against double-free

    PlatformFree(rawPtr, alignment);

    if (!m_Impl)
      return;

    m_Impl->TotalLive.fetch_sub(size, std::memory_order_relaxed);

    {
      std::lock_guard<std::mutex> lk(m_Impl->Mutex);
      auto it = m_Impl->ByLabel.find(label.Id);
      if (it != m_Impl->ByLabel.end())
      {
        it->second.LiveBytes -= size;
        it->second.Frees += 1;
      }
    }
  }
  else if (header->Magic == SystemAllocMagic)
  {
    // Pre-boot allocation -- header is valid, just free it.
    void* rawPtr = RawPtrFromHeader(header);
    u32 alignment = header->Alignment;
    header->Magic = 0;
    PlatformFree(rawPtr, alignment);
  }
  else
  {
    GECKO_ASSERT(false && "TrackingAllocator::Free: unknown allocation or double-free");
  }
}

bool TrackingAllocator::StatsFor(Label label, MemLabelStats& outStats) const
{
  if (!m_Impl)
    return false;

  std::lock_guard<std::mutex> lk(m_Impl->Mutex);
  auto it = m_Impl->ByLabel.find(label.Id);
  if (it == m_Impl->ByLabel.end())
    return false;

  outStats = it->second;
  return true;
}

::gecko::Array<MemLabelStats> TrackingAllocator::Snapshot() const
{
  ::gecko::Array<MemLabelStats> out;
  if (!m_Impl)
    return out;

  std::lock_guard<std::mutex> lk(m_Impl->Mutex);
  if (!out.Reserve(m_Impl->ByLabel.size()))
    return {};

  for (auto& [id, st] : m_Impl->ByLabel)
  {
    (void)id;
    (void)out.PushBack(st);
  }
  return out;
}

void TrackingAllocator::EmitCounters() noexcept
{}

void TrackingAllocator::ResetCounters() noexcept
{
  if (!m_Impl)
    return;

  std::lock_guard<std::mutex> lk(m_Impl->Mutex);
  for (auto& [id, st] : m_Impl->ByLabel)
  {
    st.LiveBytes = 0;
    st.Allocs = 0;
    st.Frees = 0;
  }
  m_Impl->TotalLive.store(0, std::memory_order_relaxed);
}

bool TrackingAllocator::Init() noexcept
{
  return true;
}

void TrackingAllocator::Shutdown() noexcept
{}

void TrackingAllocator::SetProfiler(IProfiler* profiler) noexcept
{
  if (m_Impl)
    m_Impl->Profiler = profiler;
}

u64 TrackingAllocator::TotalLiveBytes() const noexcept
{
  return m_Impl ? m_Impl->TotalLive.load(std::memory_order_relaxed) : 0;
}

}  // namespace gecko::runtime
