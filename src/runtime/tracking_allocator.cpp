#include "gecko/runtime/tracking_allocator.h"

#include "gecko/core/assert.h"
#include "gecko/core/services/log.h"
#include "gecko/core/services/profiler.h"
#include "private/labels.h"

#include <array>
#include <atomic>
#include <mutex>
#include <tuple>

namespace gecko::runtime {

// =============================================================================
// Thread-local allocation context
// =============================================================================
// Label stack and memory tags are thread-local

namespace {

constexpr Label g_DefaultLabel = MakeLabel("gecko.default");

struct ThreadAllocContext
{
  std::array<Label, MaxLabelStackDepth> LabelStack {};
  u32 StackDepth {0};
};

thread_local ThreadAllocContext g_AllocContext;

}  // namespace

// =============================================================================
// Label stack implementation
// =============================================================================

void TrackingAllocator::PushLabel(Label label) noexcept
{
  if (g_AllocContext.StackDepth < MaxLabelStackDepth)
  {
    g_AllocContext.LabelStack[g_AllocContext.StackDepth] = label;
    ++g_AllocContext.StackDepth;
  }
  // At max depth, we just don't push (last label is used)
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

// =============================================================================
// TrackingAllocator implementation
// =============================================================================

MemLabelStats& TrackingAllocator::EnsureLabelLocked(Label label)
{
  auto result = m_ByLabel.try_emplace(label.Id);
  auto& stats = result.first->second;
  if (result.second)
  {  // new element was inserted
    stats.StatsLabel = label;
  }
  return stats;
}

void* TrackingAllocator::Alloc(u64 size, u32 alignment) noexcept
{
  if (!m_Upstream)
  {
    return nullptr;
  }
  // NOTE: Cannot use profiling/logging - Allocator is Level 0, comes before
  // everything
  GECKO_ASSERT(m_Upstream && "Upstream allocator is required");
  GECKO_ASSERT(size > 0 && "Cannot allocate zero bytes");
  GECKO_ASSERT(alignment > 0 && (alignment & (alignment - 1)) == 0 &&
               "Alignment must be power of 2");

  // Get label from thread-local context
  const Label label = CurrentLabel();

  // Calculate total size with header
  // Layout: [padding] [AllocHeader] [user data]
  const u64 headerSize = sizeof(AllocHeader);
  const u32 effectiveAlign = alignment > alignof(AllocHeader)
                                 ? alignment
                                 : static_cast<u32>(alignof(AllocHeader));
  const u64 totalSize = headerSize + (effectiveAlign - 1) + size;

  // Allocate from upstream (which is minimal, no header)
  void* rawPtr = m_Upstream->Alloc(totalSize, effectiveAlign);
  if (!rawPtr)
    return nullptr;

  // Calculate aligned user pointer position
  auto rawAddr = reinterpret_cast<uintptr_t>(rawPtr);
  const uintptr_t alignMask = static_cast<uintptr_t>(alignment) - 1;
  uintptr_t userAddr = (rawAddr + headerSize + alignMask) & ~alignMask;

  // Place header immediately before user data
  auto* header = reinterpret_cast<AllocHeader*>(userAddr - headerSize);
  header->Magic = AllocHeaderMagic;
  header->Alignment = alignment;
  header->RequestedSize = size;
  header->AllocLabel = label;
  header->RawOffset = reinterpret_cast<uintptr_t>(header) - rawAddr;

  // Update tracking stats
  m_TotalLive.fetch_add(size, std::memory_order_relaxed);

  {
    std::lock_guard<std::mutex> lk(m_Mutex);
    auto& st = EnsureLabelLocked(label);
    st.LiveBytes.fetch_add(size, std::memory_order_relaxed);
    st.Allocs.fetch_add(1, std::memory_order_relaxed);
  }

  return reinterpret_cast<void*>(userAddr);
}

void TrackingAllocator::Free(void* ptr) noexcept
{
  if (!ptr)
    return;

  if (!m_Upstream)
  {
    return;
  }

  // Read header (immediately before user pointer)
  auto* header = HeaderFromUserPtr(ptr);

  if (!IsValidAllocHeader(header))
  {
    return;
  }

  GECKO_ASSERT(IsValidAllocHeader(header) &&
               "Invalid allocation header in TrackingAllocator::Free");

  const u64 size = header->RequestedSize;
  const Label label = header->AllocLabel;

  // Get raw pointer and clear header
  void* rawPtr = RawPtrFromHeader(header);
  header->Magic = 0;  // Detect double-free

  // Free via upstream
  if (m_Upstream)
    m_Upstream->Free(rawPtr);

  // Update tracking stats
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
{
  // NOTE: Cannot use profiling - Allocator is Layer 0, comes before Profiler
  // (Layer 2)
  // EmitCounters is a no-op now; external systems can call TotalLiveBytes() or
  // Snapshot() and profile those values themselves
}

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
