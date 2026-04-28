#include "gecko/runtime/tracking_allocator.h"

#include "gecko/core/assert.h"
#include "gecko/core/services/log.h"
#include "gecko/core/services/profiler.h"
#include "private/labels.h"

#include <array>
#include <atomic>
#include <mutex>

namespace gecko::runtime {

//------------------------------------------------------------
// Thread-local allocation context
//------------------------------------------------------------

namespace {
constexpr Label g_DefaultLabel = MakeLabel("gecko.default");

struct ThreadAllocContext
{
  std::array<Label, MaxLabelStackDepth> LabelStack {};
  u32 StackDepth {0};
};

thread_local ThreadAllocContext g_AllocContext;

}  // namespace

//------------------------------------------------------------
// Label Stack
//------------------------------------------------------------

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

//------------------------------------------------------------
// Alloc / Free
//------------------------------------------------------------

MemLabelStats& TrackingAllocator::EnsureLabelLocked(Label label)
{
  auto result = m_ByLabel.try_emplace(label.Id);
  auto& stats = result.first->second;
  if (result.second)
  {
    stats.StatsLabel = label;
  }
  return stats;
}

void* TrackingAllocator::Alloc(u64 size, u32 alignment) noexcept
{
  GECKO_ASSERT(size > 0 && "Cannot allocate zero bytes");
  GECKO_ASSERT(alignment > 0 && (alignment & (alignment - 1)) == 0 &&
               "Alignment must be power of 2");

  const Label label = CurrentLabel();
  const u32 effAlign = EffectiveAlignment(alignment);
  const u64 totalSize = TotalAllocSize(size, alignment);

  void* rawPtr = PlatformAlloc(totalSize, effAlign);
  if (!rawPtr)
    return nullptr;

  void* userPtr =
      PlaceAllocHeader(rawPtr, size, alignment, TrackingAllocMagic, label);

  m_TotalLive.fetch_add(size, std::memory_order_relaxed);

  {
    std::lock_guard<std::mutex> lk(m_Mutex);
    auto& st = EnsureLabelLocked(label);
    st.LiveBytes.fetch_add(size, std::memory_order_relaxed);
    st.Allocs.fetch_add(1, std::memory_order_relaxed);
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

    m_TotalLive.fetch_sub(size, std::memory_order_relaxed);

    {
      std::lock_guard<std::mutex> lk(m_Mutex);
      auto it = m_ByLabel.find(label.Id);
      if (it != m_ByLabel.end())
      {
        it->second.LiveBytes.fetch_sub(size, std::memory_order_relaxed);
        it->second.Frees.fetch_add(1, std::memory_order_relaxed);
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
    GECKO_ASSERT(false &&
                 "TrackingAllocator::Free: unknown allocation or double-free");
  }
}

bool TrackingAllocator::StatsFor(Label label, MemLabelStats& outStats) const
{
  std::lock_guard<std::mutex> lk(m_Mutex);
  auto it = m_ByLabel.find(label.Id);
  if (it == m_ByLabel.end())
    return false;

  outStats.StatsLabel = it->second.StatsLabel;
  outStats.LiveBytes.store(it->second.LiveBytes.load(std::memory_order_relaxed),
                           std::memory_order_relaxed);
  outStats.Allocs.store(it->second.Allocs.load(std::memory_order_relaxed),
                        std::memory_order_relaxed);
  outStats.Frees.store(it->second.Frees.load(std::memory_order_relaxed),
                       std::memory_order_relaxed);

  return true;
}

void TrackingAllocator::Snapshot(
    std::unordered_map<u64, MemLabelStats>& out) const
{
  std::lock_guard<std::mutex> lk(m_Mutex);
  out.clear();
  out.reserve(m_ByLabel.size());
  for (auto& [id, st] : m_ByLabel)
  {
    auto result = out.try_emplace(id);
    auto& snap = result.first->second;
    snap.StatsLabel = st.StatsLabel;
    snap.LiveBytes.store(st.LiveBytes.load(std::memory_order_relaxed),
                         std::memory_order_relaxed);
    snap.Allocs.store(st.Allocs.load(std::memory_order_relaxed),
                      std::memory_order_relaxed);
    snap.Frees.store(st.Frees.load(std::memory_order_relaxed),
                     std::memory_order_relaxed);
  }
}

void TrackingAllocator::EmitCounters() noexcept
{}

void TrackingAllocator::ResetCounters() noexcept
{
  std::lock_guard<std::mutex> lk(m_Mutex);
  for (auto& [id, st] : m_ByLabel)
  {
    st.LiveBytes.store(0, std::memory_order_relaxed);
    st.Allocs.store(0, std::memory_order_relaxed);
    st.Frees.store(0, std::memory_order_relaxed);
  }
  m_TotalLive.store(0, std::memory_order_relaxed);
}

bool TrackingAllocator::Init() noexcept
{
  return true;
}

void TrackingAllocator::Shutdown() noexcept
{}

}  // namespace gecko::runtime
