#include "gecko/runtime/tracking_allocator.h"

#include <atomic>
#include <mutex>
#include <tuple>

#include "gecko/core/assert.h"
#include "gecko/core/profiler.h"

#include "labels.h"

namespace gecko::runtime {

MemLabelStats &TrackingAllocator::EnsureLabelLocked(Label label) {
  auto result = m_ByLabel.try_emplace(label.Id);
  auto &stats = result.first->second;
  if (result.second) { // new element was inserted
    stats.StatsLabel = label;
  }
  return stats;
}

void *TrackingAllocator::Alloc(u64 size, u32 alignment, Label label) noexcept {
  GECKO_ASSERT(m_Upstream && "Upstream allocator is required");
  GECKO_ASSERT(size > 0 && "Cannot allocate zero bytes");
  GECKO_ASSERT(alignment > 0 && (alignment & (alignment - 1)) == 0 &&
               "Alignment must be power of 2");

  void *ptr = m_Upstream->Alloc(size, alignment, label);
  if (!ptr)
    return nullptr;

  m_TotalLive.fetch_add(size, std::memory_order_relaxed);

  {
    std::lock_guard<std::mutex> lk(m_Mutex);
    auto &st = EnsureLabelLocked(label);
    st.LiveBytes.fetch_add(size, std::memory_order_relaxed);
    st.Allocs.fetch_add(1, std::memory_order_relaxed);
  }

  return ptr;
}

void TrackingAllocator::Free(void *ptr, u64 size, u32 alignment,
                             Label label) noexcept {
  if (!ptr)
    return;

  if (m_Upstream)
    m_Upstream->Free(ptr, size, alignment, label);

  m_TotalLive.fetch_sub(size, std::memory_order_relaxed);

  {
    std::lock_guard<std::mutex> lk(m_Mutex);
    auto it = m_ByLabel.find(label.Id);
    if (it != m_ByLabel.end()) {
      it->second.LiveBytes.fetch_sub(size, std::memory_order_relaxed);
      it->second.Frees.fetch_add(1, std::memory_order_relaxed);
    }
  }
}

bool TrackingAllocator::StatsFor(Label label, MemLabelStats &outStats) const {
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
    std::unordered_map<u64, MemLabelStats> &out) const {
  std::lock_guard<std::mutex> lk(m_Mutex);
  out.clear();
  out.reserve(m_ByLabel.size());
  for (auto &[id, st] : m_ByLabel) {
    auto result = out.try_emplace(id);
    auto &snap = result.first->second;
    snap.StatsLabel = st.StatsLabel;
    snap.LiveBytes.store(st.LiveBytes.load(std::memory_order_relaxed),
                         std::memory_order_relaxed);
    snap.Allocs.store(st.Allocs.load(std::memory_order_relaxed),
                      std::memory_order_relaxed);
    snap.Frees.store(st.Frees.load(std::memory_order_relaxed),
                     std::memory_order_relaxed);
  }
}

void TrackingAllocator::EmitCounters() noexcept {
  if (!m_Profiler)
    return;

  GECKO_PROF_COUNTER(labels::TrackingAllocator, "heap_live_bytes",
                     TotalLiveBytes());

  std::unordered_map<u64, MemLabelStats> snap;
  Snapshot(snap);
  for (auto &[id, st] : snap) {
    const char *name = st.StatsLabel.Name ? st.StatsLabel.Name : "mem";
    GECKO_PROF_COUNTER(st.StatsLabel, name,
                       st.LiveBytes.load(std::memory_order_relaxed));
  }
}

void TrackingAllocator::ResetCounters() noexcept {
  std::lock_guard<std::mutex> lk(m_Mutex);
  for (auto &[id, st] : m_ByLabel) {
    st.LiveBytes.store(0, std::memory_order_relaxed);
    st.Allocs.store(0, std::memory_order_relaxed);
    st.Frees.store(0, std::memory_order_relaxed);
  }
  m_TotalLive.store(0, std::memory_order_relaxed);
}

bool TrackingAllocator::Init() noexcept { return true; }

void TrackingAllocator::Shutdown() noexcept {}

} // namespace gecko::runtime
