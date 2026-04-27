#pragma once

#if defined(GECKO_GRAPHICS_VULKAN)

#include "gecko/graphics/gpu_profiler.h"
#include "gecko/graphics/graphics_device.h"
#include "gecko/graphics/graphics_types.h"

#include <vector>

namespace gecko::graphics {

class VulkanDevice;

// V1 GPU sampler.
//
// One QueryPool per in-flight frame slot. Each frame's pool holds
// (1 + MaxZonesPerFrame*2) timestamps:
//   slot 0           : frame-start
//   slot 1..2N       : zone begin/end pairs
//
// EndFrame walks the slot that is FramesInFlight-1 steps behind the
// current one (assumed signalled) and emits its zones into IProfiler.
//
// This is intentionally simple: no CPU↔GPU clock alignment, raw GPU
// timestamps are emitted on a synthetic `GpuThreadId` row. Refinement
// (anchoring per-frame to a CPU NowNs sample) can land on top later.
class VulkanGpuSampler final : public IGpuSampler
{
public:
  VulkanGpuSampler(VulkanDevice& device, const GpuSamplerDesc& desc) noexcept;
  ~VulkanGpuSampler() override;

  void BeginFrame(ICommandList& cmd) noexcept override;
  void EndFrame(ICommandList& cmd) noexcept override;
  void BeginZone(ICommandList& cmd, ::gecko::Label label, const char* name,
                 ::gecko::ProfLevel level) noexcept override;
  void EndZone(ICommandList& cmd) noexcept override;
  void OnSubmit(u64 cpuNowNs) noexcept override;

  [[nodiscard]] bool IsValid() const noexcept
  {
    return m_Valid;
  }

private:
  static constexpr u32 c_MaxNestingDepth = 32;

  struct ZoneRecord
  {
    ::gecko::Label ScopeLabel {};
    const char* Name {nullptr};
    u32 NameHash {0};
    u32 BeginQuery {0};
    u32 EndQuery {0};
    ::gecko::ProfLevel Level {::gecko::ProfLevel::Normal};
  };

  struct FrameSlot
  {
    QueryPool Pool {};
    std::vector<ZoneRecord> Zones {};
    u32 NextQuery {0};
    bool Pending {false};     // submitted, awaiting resolve
    u64 CpuFrameStartNs {0};  // CPU NowNs sampled at BeginFrame
    u64 SubmitCpuNs {0};      // CPU NowNs at first vkQueueSubmit for slot
  };

  void ResolveSlot(FrameSlot& slot) noexcept;

  VulkanDevice* m_Device {nullptr};
  GpuSamplerDesc m_Desc {};
  std::vector<FrameSlot> m_Frames;
  u32 m_Current {0};

  // Open-zone stack, holds indices into the current FrameSlot's Zones.
  u32 m_OpenStack[c_MaxNestingDepth] {};
  u32 m_StackDepth {0};

  bool m_Valid {false};
};

}  // namespace gecko::graphics

#endif  // GECKO_GRAPHICS_VULKAN
