#pragma once
#include <atomic>
#include <vector>

#include "gecko/core/core.h"

namespace gecko::runtime {

  class RingProfiler final : public IProfiler
  {
  public:

    explicit RingProfiler(size_t capacityPow2 = 1u << 20);
    ~RingProfiler();

    void Emit(const ProfEvent& event) noexcept override;
    u64 NowNs() const noexcept override;

    bool TryPop(ProfEvent& event) noexcept;

  private:
    struct Slot {
      std::atomic<u64> Sequence { 0 };
      ProfEvent ProfileEvent { };
    };
    std::vector<Slot> m_Ring { };
    size_t m_Mask { 0 };
    std::atomic<u64> m_Head { 0 };
    std::atomic<u64> m_Tail { 0 };

    static u64 MonotonicNowNs() noexcept;
  };

  u32 ThisThreadId() noexcept;

}
