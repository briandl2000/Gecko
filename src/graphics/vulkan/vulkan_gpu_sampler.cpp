#if defined(GECKO_GRAPHICS_VULKAN)
#include "vulkan_gpu_sampler.h"

#include "gecko/core/services/log.h"
#include "gecko/core/utility/hash.h"
#include "private/labels.h"
#include "vulkan_device.h"

namespace gecko::graphics {

VulkanGpuSampler::VulkanGpuSampler(VulkanDevice& device,
                                   const GpuSamplerDesc& desc) noexcept
    : m_Device(&device), m_Desc(desc)
{
  // Each frame needs (1 frame-start) + (2 per zone) timestamps.
  const u32 timestampsPerFrame = 1U + 2U * m_Desc.MaxZonesPerFrame;
  const u32 frames = m_Desc.FramesInFlight == 0 ? 1U : m_Desc.FramesInFlight;

  m_Frames.resize(frames);
  for (u32 i = 0; i < frames; ++i)
  {
    QueryPoolDesc qpDesc {};
    qpDesc.Count = timestampsPerFrame;
    qpDesc.DebugName = "GpuSampler.QueryPool";
    m_Frames[i].Pool = device.CreateTimestampQueryPool(qpDesc);
    if (!m_Frames[i].Pool.IsValid())
    {
      GECKO_ERROR(labels::Vulkan,
                  "VulkanGpuSampler: CreateTimestampQueryPool failed");
      return;
    }
    m_Frames[i].Zones.reserve(m_Desc.MaxZonesPerFrame);
  }

  // Register the synthetic GPU thread name so trace sinks show "GPU" on
  // its row instead of a numeric id.
  if (m_Desc.GpuThreadName != nullptr)
    ::gecko::RegisterThreadProfilerName(m_Desc.GpuThreadId,
                                        m_Desc.GpuThreadName);

  m_Valid = true;
}

VulkanGpuSampler::~VulkanGpuSampler()
{
  // QueryPool destructor releases the VkQueryPool.
}

void VulkanGpuSampler::BeginFrame(ICommandList& cmd) noexcept
{
  if (!m_Valid)
    return;

  FrameSlot& slot = m_Frames[m_Current];

  // If this slot is still pending (sampler under-rotated), drop its
  // contents to avoid reading stale or in-flight queries.
  slot.Zones.clear();
  slot.NextQuery = 0;
  m_StackDepth = 0;

  // Sample the CPU clock now so we can rebase the GPU timeline against
  // it when ResolveSlot runs. This makes GPU events appear in the same
  // monotonic time domain as CPU events in the trace.
  if (auto* p = ::gecko::GetProfiler(); p != nullptr)
    slot.CpuFrameStartNs = p->NowNs();
  else
    slot.CpuFrameStartNs = 0;

  const u32 timestampsPerFrame = 1U + 2U * m_Desc.MaxZonesPerFrame;
  cmd.ResetTimestamps(slot.Pool, 0, timestampsPerFrame);
  cmd.WriteTimestamp(slot.Pool, slot.NextQuery++);
  slot.Pending = true;
}

void VulkanGpuSampler::EndFrame(ICommandList& cmd) noexcept
{
  if (!m_Valid)
    return;

  // Closing any stuck-open zones is unsafe (the timestamps were already
  // written for them) so we just rotate. EndZone-mismatch is a caller bug.
  m_StackDepth = 0;

  FrameSlot& slot = m_Frames[m_Current];
  // Reserve the last timestamp slot for frame end (optional, currently
  // unused for emission but useful for future per-frame zone).
  if (slot.NextQuery < (1U + 2U * m_Desc.MaxZonesPerFrame))
    cmd.WriteTimestamp(slot.Pool, slot.NextQuery++);

  // Advance the ring; resolve the slot we are about to wrap around to.
  m_Current = (m_Current + 1U) % static_cast<u32>(m_Frames.size());

  FrameSlot& resolveSlot = m_Frames[m_Current];
  if (resolveSlot.Pending)
    ResolveSlot(resolveSlot);
}

void VulkanGpuSampler::BeginZone(ICommandList& cmd, ::gecko::Label label,
                                 const char* name) noexcept
{
  if (!m_Valid)
    return;

  FrameSlot& slot = m_Frames[m_Current];

  if (slot.Zones.size() >= m_Desc.MaxZonesPerFrame)
    return;
  if (m_StackDepth >= c_MaxNestingDepth)
    return;
  if (slot.NextQuery + 2U > (1U + 2U * m_Desc.MaxZonesPerFrame))
    return;

  ZoneRecord rec {};
  rec.ScopeLabel = label;
  rec.Name = name;
  rec.NameHash = ::gecko::FNV1a(name);
  rec.BeginQuery = slot.NextQuery++;
  rec.EndQuery = 0;

  cmd.WriteTimestamp(slot.Pool, rec.BeginQuery);

  slot.Zones.push_back(rec);
  m_OpenStack[m_StackDepth++] = static_cast<u32>(slot.Zones.size() - 1U);
}

void VulkanGpuSampler::EndZone(ICommandList& cmd) noexcept
{
  if (!m_Valid || m_StackDepth == 0)
    return;

  FrameSlot& slot = m_Frames[m_Current];
  const u32 zoneIdx = m_OpenStack[--m_StackDepth];
  if (zoneIdx >= slot.Zones.size())
    return;

  ZoneRecord& rec = slot.Zones[zoneIdx];
  rec.EndQuery = slot.NextQuery++;
  cmd.WriteTimestamp(slot.Pool, rec.EndQuery);
}

void VulkanGpuSampler::ResolveSlot(FrameSlot& slot) noexcept
{
  slot.Pending = false;

  if (slot.Zones.empty() || slot.NextQuery == 0)
    return;

  // Pull all timestamps in one shot.
  std::vector<u64> ts(slot.NextQuery, 0);
  const u32 got = m_Device->ReadTimestamps(
      slot.Pool, 0, std::span<u64>(ts.data(), ts.size()));
  if (got == 0)
    return;

  auto* p = ::gecko::GetProfiler();
  if (p == nullptr)
    return;

  // Convert raw GPU ticks → nanoseconds, then rebase to CPU frame start.
  // This places GPU zones inside the wall-clock window of the frame in
  // which they were submitted (good enough for visualisation; for true
  // CPU↔GPU calibration we'd need vkGetCalibratedTimestampsEXT).
  const f64 period = static_cast<f64>(m_Device->TimestampPeriodNs());
  const u64 gpuFrameStart = ts[0];
  const u64 cpuFrameStart = slot.CpuFrameStartNs;
  auto rebase = [&](u64 tick) noexcept -> u64 {
    const f64 deltaNs = static_cast<f64>(tick - gpuFrameStart) * period;
    return cpuFrameStart + static_cast<u64>(deltaNs);
  };

  for (const ZoneRecord& rec : slot.Zones)
  {
    if (rec.EndQuery == 0)  // never closed
      continue;
    if (rec.BeginQuery >= got || rec.EndQuery >= got)
      continue;

    const u64 beginNs = rebase(ts[rec.BeginQuery]);
    const u64 endNs = rebase(ts[rec.EndQuery]);

    ::gecko::ProfEvent ev {};
    ev.TimestampNs = beginNs;
    ev.Name = rec.Name;
    ev.EventLabel = rec.ScopeLabel;
    ev.ThreadId = m_Desc.GpuThreadId;
    ev.NameHash = rec.NameHash;
    ev.Kind = ::gecko::ProfEventKind::ZoneBegin;
    ev.Source = ::gecko::ProfSource::GPU;
    p->Emit(ev);

    ev.TimestampNs = endNs;
    ev.Kind = ::gecko::ProfEventKind::ZoneEnd;
    p->Emit(ev);
  }

  slot.Zones.clear();
  slot.NextQuery = 0;
}

}  // namespace gecko::graphics
#endif
