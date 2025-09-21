#include "gecko/runtime/ring_profiler.h"
#include <chrono>
#include <thread>

namespace gecko {

  static inline u32 HashTid() 
  {
    auto id = std::hash<std::thread::id>{}(std::this_thread::get_id());
    return (u32)(id ^ (id >> 32));
  }

  u32 ThisThreadId() noexcept { return HashTid(); }
}

namespace gecko::runtime {

  u64 RingProfiler::MonotonicNowNs() noexcept 
  {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
  }

  RingProfiler::RingProfiler(size_t capacityPow2)
    : m_Ring(capacityPow2)
    , m_Mask(capacityPow2 - 1)
    , m_Head(0)
    , m_Tail(0)
  {
    if((capacityPow2 & (capacityPow2 - 1)) != 0) 
    {
      m_Mask = (1u<<20) - 1;
      m_Ring = std::vector<Slot>(1u<<20);
    }
    for (u64 i = 0; i < m_Ring.size(); ++i)
    {
      m_Ring[i].Sequence.store(i, std::memory_order_relaxed);
    }
  }

  RingProfiler::~RingProfiler() = default;

  u64 RingProfiler::NowNs() const noexcept { return MonotonicNowNs(); }

  void RingProfiler::Emit(const ProfEvent& ev) noexcept
  {
    u64 pos = m_Head.fetch_add(1, std::memory_order_acq_rel);
    Slot& slot = m_Ring[pos & m_Mask];

    u64 sequence = slot.Sequence.load(std::memory_order_acquire);
    i64 diff = (i64)sequence - (i64)pos;
    if (diff == 0) {
      slot.ProfileEvent = ev; // copy event
      slot.Sequence.store(pos + 1, std::memory_order_release);
    } else {
      // overflow — drop event (cheap fallback)
      // Optionally: back off or count drops.
    }
  }

  bool RingProfiler::TryPop(ProfEvent& outEvent) noexcept
  {
    u64 pos = m_Tail.load(std::memory_order_relaxed);
    Slot& slot = m_Ring[pos & m_Mask];
    u64 sequence = slot.Sequence.load(std::memory_order_acquire);
    i64 diff = (i64)sequence - (i64)(pos + 1);
    if (diff == 0) {
      outEvent = slot.ProfileEvent;
      slot.Sequence.store(pos + m_Ring.size(), std::memory_order_release);
      m_Tail.store(pos + 1, std::memory_order_relaxed);
      return true;
    }
    return false;
}

}
