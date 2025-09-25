#include "gecko/runtime/tracking_allocator.h"

#include <atomic>
#include <mutex>
#include <tuple>

#include "categories.h"
#include "gecko/core/assert.h"
#include "gecko/core/profiler.h"

namespace gecko::runtime {

  MemCategoryStats& TrackingAllocator::EnsureCategoryLocked(Category category) 
  {
    auto result = m_ByCat.try_emplace(category.Id);
    auto& stats = result.first->second;
    if (result.second) { // new element was inserted
      stats.Cat = category;
    }
    return stats;
  }

  void* TrackingAllocator::Alloc(u64 size, u32 alignment, Category category) noexcept 
  {
    GECKO_ASSERT(m_Upstream && "Upstream allocator is required");
    GECKO_ASSERT(size > 0 && "Cannot allocate zero bytes");
    GECKO_ASSERT(alignment > 0 && (alignment & (alignment - 1)) == 0 && "Alignment must be power of 2");
    
    void* ptr = m_Upstream->Alloc(size, alignment, category);
    if (!ptr) return nullptr;

    m_TotalLive.fetch_add(size, std::memory_order_relaxed);

    {
      std::lock_guard<std::mutex> lk(m_Mutex);
      auto& st = EnsureCategoryLocked(category);
      st.LiveBytes.fetch_add(size, std::memory_order_relaxed);
      st.Allocs.fetch_add(1, std::memory_order_relaxed);
    }

    return ptr;
  }

  void TrackingAllocator::Free(void* ptr, u64 size, u32 alignment, Category category) noexcept
  {
    if (!ptr) return;

    if (m_Upstream) m_Upstream->Free(ptr, size, alignment, category);

    m_TotalLive.fetch_sub(size, std::memory_order_relaxed);

    {
      std::lock_guard<std::mutex> lk(m_Mutex);
      auto it = m_ByCat.find(category.Id);
      if (it != m_ByCat.end()) 
      {
        it->second.LiveBytes.fetch_sub(size, std::memory_order_relaxed);
        it->second.Frees.fetch_add(1, std::memory_order_relaxed);
      }
    }
  }

  bool TrackingAllocator::StatsFor(Category category, MemCategoryStats& outStats) const
  {
    std::lock_guard<std::mutex> lk(m_Mutex);
    auto it = m_ByCat.find(category.Id);
    if (it == m_ByCat.end()) return false;

    outStats.Cat = it->second.Cat;
    outStats.LiveBytes.store(it->second.LiveBytes.load(std::memory_order_relaxed), std::memory_order_relaxed);
    outStats.Allocs.store(it->second.Allocs.load(std::memory_order_relaxed), std::memory_order_relaxed);
    outStats.Frees.store(it->second.Frees.load(std::memory_order_relaxed), std::memory_order_relaxed);

    return true;
  }

  void TrackingAllocator::Snapshot(std::unordered_map<u32, MemCategoryStats>& out) const
  {
    std::lock_guard<std::mutex> lk(m_Mutex);
    out.clear();
    out.reserve(m_ByCat.size());
    for (auto& [id, st] : m_ByCat)
    {
      auto result = out.try_emplace(id);
      auto& snap = result.first->second;
      snap.Cat = st.Cat;
      snap.LiveBytes.store(st.LiveBytes.load(std::memory_order_relaxed), std::memory_order_relaxed);
      snap.Allocs.store(st.Allocs.load(std::memory_order_relaxed), std::memory_order_relaxed);
      snap.Frees.store(st.Frees.load(std::memory_order_relaxed), std::memory_order_relaxed);
    } 
  }

  void TrackingAllocator::EmitCounters() noexcept 
  {
    if (!m_Profiler) return;

    GECKO_PROF_COUNTER(categories::TrackingAllocator, "heap_live_bytes", TotalLiveBytes());

    std::unordered_map<u32, MemCategoryStats> snap;
    Snapshot(snap);
    for (auto& [id, st] : snap)
    {
      const char* name = st.Cat.Name ? st.Cat.Name : "mem";

      GECKO_PROF_COUNTER(st.Cat, name, st.LiveBytes.load(std::memory_order_relaxed));
    }
  }

  void TrackingAllocator::ResetCounters() noexcept
  {
    std::lock_guard<std::mutex> lk(m_Mutex);
    for (auto& [id, st] : m_ByCat)
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
  {

  }

}
