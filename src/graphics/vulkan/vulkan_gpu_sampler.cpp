#if defined(GECKO_GRAPHICS_VULKAN)
#include "vulkan_gpu_sampler.h"

#include "../private/labels.h"
#include "gecko/core/services/log.h"
#include "gecko/core/utility/hash.h"
#include "vulkan_device.h"

namespace gecko::graphics {

VulkanGpuSampler::VulkanGpuSampler(VulkanDevice& device, const GpuSamplerDesc& desc) noexcept
    : m_Device(&device), m_Desc(desc)
{
  // Each frame needs 2 timestamps per zone (begin + end).
  const u32 timestampsPerFrame = 2U * m_Desc.MaxZonesPerFrame;
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
      GECKO_ERROR(labels::Vulkan, "VulkanGpuSampler: CreateTimestampQueryPool failed");
      return;
    }
    m_Frames[i].Zones.reserve(m_Desc.MaxZonesPerFrame);
  }

  // Register the synthetic GPU thread name so trace sinks show "GPU" on
  // its row instead of a numeric id.
  if (m_Desc.GpuThreadName != nullptr)
    gecko::RegisterThreadProfilerName(m_Desc.GpuThreadId, m_Desc.GpuThreadName);

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

  // Advance the ring; resolve the slot we are about to wrap into (its
  // GPU work from FramesInFlight frames ago is now assumed signalled).
  // Doing rotation in BeginFrame (rather than EndFrame) lets EndZone
  // calls fire after EndFrame -- e.g. the graphics command list's
  // auto 'CommandList' Always zone is closed by cmd->End() which the
  // app calls after gpuSampler->EndFrame.
  if (m_Frames[m_Current].Pending)
  {
    m_Current = (m_Current + 1U) % static_cast<u32>(m_Frames.size());
    FrameSlot& resolveSlot = m_Frames[m_Current];
    if (resolveSlot.Pending)
      ResolveSlot(resolveSlot);
  }

  FrameSlot& slot = m_Frames[m_Current];

  // If this slot is still pending (sampler under-rotated), drop its
  // contents to avoid reading stale or in-flight queries.
  slot.Zones.clear();
  slot.NextQuery = 0;
  m_StackDepth = 0;
  slot.SubmitCpuNs = 0;

  // Sample the CPU clock now as a fallback rebase anchor. This is used
  // only if no OnSubmit() arrives before ResolveSlot runs (e.g. cmd
  // lists were never submitted). When OnSubmit fires we prefer that
  // timestamp, since it lines up with the actual vkQueueSubmit call.
  if (auto* p = gecko::GetProfiler(); p != nullptr)
    slot.CpuFrameStartNs = p->NowNs();
  else
    slot.CpuFrameStartNs = 0;

  // No cmd-side ResetTimestamps here -- the pool was reset from the host
  // either at creation (first use) or after ResolveSlot (subsequent
  // uses). Embedding the reset on `cmd` would race with cmd lists
  // submitted later in record order but earlier in GPU-execution order
  // (e.g. compute cmd lists submitted before the graphics cmd that
  // recorded BeginFrame).
  (void)cmd;
  slot.Pending = true;
}

void VulkanGpuSampler::EndFrame(ICommandList& cmd) noexcept
{
  if (!m_Valid)
    return;
  // Rotation moved to BeginFrame so that EndZone calls firing after
  // EndFrame (e.g. the graphics cmd's auto 'CommandList' zone closed
  // by cmd->End()) still target the correct slot. Closing any
  // stuck-open zones here would be unsafe (timestamps already
  // written), so EndFrame is now effectively a no-op.
  (void)cmd;
}

void VulkanGpuSampler::BeginZone(ICommandList& cmd, gecko::Label label, const char* name,
                                 gecko::ProfLevel level) noexcept
{
  if (!m_Valid)
    return;

  FrameSlot& slot = m_Frames[m_Current];

  if (slot.Zones.size() >= m_Desc.MaxZonesPerFrame)
    return;
  if (m_StackDepth >= MaxNestingDepth)
    return;
  if (slot.NextQuery + 2U > 2U * m_Desc.MaxZonesPerFrame)
    return;

  ZoneRecord rec {};
  rec.ScopeLabel = label;
  rec.Name = name;
  rec.NameHash = gecko::FNV1a(name);
  rec.BeginQuery = slot.NextQuery++;
  rec.EndQuery = 0;
  rec.Level = level;

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

void VulkanGpuSampler::OnSubmit(u64 cpuNowNs) noexcept
{
  if (!m_Valid)
    return;
  // Record the CPU timestamp of the first submit covering the slot
  // currently being recorded. Subsequent submits in the same frame
  // don't override it -- only the first submit's CPU time anchors the
  // GPU timeline so its earliest GPU timestamp lands on the
  // vkQueueSubmit call site.
  FrameSlot& slot = m_Frames[m_Current];
  if (slot.SubmitCpuNs == 0)
    slot.SubmitCpuNs = cpuNowNs;
}

void VulkanGpuSampler::ResolveSlot(FrameSlot& slot) noexcept
{
  slot.Pending = false;

  const u32 poolSize = 2U * m_Desc.MaxZonesPerFrame;

  if (slot.Zones.empty() || slot.NextQuery == 0)
  {
    m_Device->HostResetQueryPool(slot.Pool, 0, poolSize);
    return;
  }

  // Pull all timestamps in one shot. ReadTimestamps converts ticks->ns
  // internally using the device timestamp period.
  Array<u64> ts(slot.NextQuery, 0);
  const u32 got = m_Device->ReadTimestamps(slot.Pool, 0, Span<u64>(ts.data(), ts.size()));
  if (got == 0)
  {
    m_Device->HostResetQueryPool(slot.Pool, 0, poolSize);
    return;
  }

  auto* p = gecko::GetProfiler();
  if (p == nullptr)
  {
    m_Device->HostResetQueryPool(slot.Pool, 0, poolSize);
    return;
  }

  // Use the smallest valid GPU timestamp as the anchor. Recording order
  // does not match GPU execution order when cmd lists are submitted out
  // of recording order (e.g. compute cmds submitted before a graphics
  // cmd whose BeginFrame call was already recorded), so a fixed anchor
  // like ts[0] could lead to underflow when computing deltas.
  u64 gpuFrameStart = ~0ULL;
  for (const ZoneRecord& rec : slot.Zones)
  {
    if (rec.EndQuery == 0)
      continue;
    if (rec.BeginQuery >= got || rec.EndQuery >= got)
      continue;
    if (ts[rec.BeginQuery] < gpuFrameStart)
      gpuFrameStart = ts[rec.BeginQuery];
  }
  if (gpuFrameStart == ~0ULL)
  {
    m_Device->HostResetQueryPool(slot.Pool, 0, poolSize);
    return;
  }
  // Prefer the first vkQueueSubmit CPU timestamp as the rebase anchor
  // (closest to actual GPU work start). Fall back to BeginFrame time
  // only if OnSubmit was never called (e.g. cmd lists never submitted).
  const u64 cpuAnchor = slot.SubmitCpuNs != 0 ? slot.SubmitCpuNs : slot.CpuFrameStartNs;
  auto rebase = [&](u64 ns) noexcept -> u64 { return cpuAnchor + (ns - gpuFrameStart); };

  for (const ZoneRecord& rec : slot.Zones)
  {
    if (rec.EndQuery == 0)  // never closed
      continue;
    if (rec.BeginQuery >= got || rec.EndQuery >= got)
      continue;

    const u64 beginNs = rebase(ts[rec.BeginQuery]);
    const u64 endNs = rebase(ts[rec.EndQuery]);

    gecko::ProfEvent ev {};
    ev.TimestampNs = beginNs;
    ev.Name = rec.Name;
    ev.EventLabel = rec.ScopeLabel;
    ev.ThreadId = m_Desc.GpuThreadId;
    ev.NameHash = rec.NameHash;
    ev.Kind = gecko::ProfEventKind::ZoneBegin;
    ev.Source = gecko::ProfSource::GPU;
    ev.Level = rec.Level;
    p->Emit(ev);

    ev.TimestampNs = endNs;
    ev.Kind = gecko::ProfEventKind::ZoneEnd;
    p->Emit(ev);
  }

  slot.Zones.clear();
  slot.NextQuery = 0;

  // Host-reset the pool so it's ready for re-use on the next ring wrap.
  m_Device->HostResetQueryPool(slot.Pool, 0, poolSize);
}

}  // namespace gecko::graphics
#endif
