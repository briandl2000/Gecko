#include "gecko/debug_renderer/debug_renderer_context.h"

#include "gecko/core/services/log.h"
#include "gecko/graphics/graphics_module.h"
#include "private/labels.h"
#include "private/types.h"

#include <algorithm>

namespace gecko::debug_renderer {

DebugRendererContext::DebugRendererContext(::gecko::u32 lineCapacity,
                                           ::gecko::u32 framesInFlight)
{
  if (lineCapacity == 0)
  {
    GECKO_ERROR(labels::Context, "lineCapacity must be > 0");
    return;
  }
  if (framesInFlight == 0 || framesInFlight > MaxFramesInFlight)
  {
    GECKO_ERROR(labels::Context, "framesInFlight ({}) must be in [1, {}]",
                framesInFlight, MaxFramesInFlight);
    return;
  }

  auto* device = ::gecko::graphics::GetGraphicsDevice();
  if (!device)
  {
    GECKO_ERROR(labels::Context, "GraphicsModule did not publish a device");
    return;
  }

  m_LineCapacity = lineCapacity;
  m_FrameSlots.resize(framesInFlight);

  ::gecko::graphics::StructuredBufferDesc desc {};
  desc.ElementSize = sizeof(Line2D);
  desc.NumElements = lineCapacity;
  desc.Memory = ::gecko::graphics::MemoryType::Dedicated;

  for (auto& slot : m_FrameSlots)
  {
    slot.CPU.resize(lineCapacity);
    slot.GPU = device->CreateStructuredBuffer(desc);
    if (!slot.GPU.IsValid())
    {
      GECKO_ERROR(labels::Context, "Failed to create per-frame line buffer");
      m_FrameSlots.clear();
      return;
    }
  }

  m_Valid = true;
}

void DebugRendererContext::NewFrame()
{
  if (!m_Valid)
    return;

  if (m_FrameStarted && !m_FrameUploaded && m_LineCursor > 0)
  {
    GECKO_WARN(labels::Context,
               "NewFrame called before EndFrame; previous frame's {} line(s) "
               "were never uploaded to the GPU and will not appear",
               m_LineCursor);
  }

  m_CurrentSlot = (m_CurrentSlot + 1) % m_FrameSlots.size();
  m_LineCursor = 0;
  m_BatchStart = 0;
  m_FrameStarted = true;
  m_FrameUploaded = false;
  m_LineOverflowWarned = false;
}

void DebugRendererContext::DrawLine(::gecko::math::float2 a,
                                    ::gecko::math::float2 b,
                                    ::gecko::math::float3 color,
                                    ::gecko::f32 thickness)
{
  if (!m_Valid)
    return;

  if (m_LineCursor >= m_LineCapacity)
  {
    if (!m_LineOverflowWarned)
    {
      GECKO_WARN(labels::Context,
                 "Exceeded debug line buffer capacity ({}); subsequent lines "
                 "this frame will be dropped",
                 m_LineCapacity);
      m_LineOverflowWarned = true;
    }
    return;
  }

  auto& slot = m_FrameSlots[m_CurrentSlot];
  slot.CPU[m_LineCursor++] = Line2D {a, b, color, thickness};
}

void DebugRendererContext::Submit(::gecko::graphics::ICommandList* cmd,
                                  const DebugRendererSubmitInfo& info)
{
  if (!cmd || !m_Valid)
    return;
  if (!m_FrameStarted)
  {
    GECKO_WARN(labels::Context, "Submit called before NewFrame; ignored");
    return;
  }

  const ::gecko::u32 batchSize = m_LineCursor - m_BatchStart;
  if (batchSize == 0)
    return;

  auto& slot = m_FrameSlots[m_CurrentSlot];

  cmd->BindPipeline(GetDebugLinePipeline());
  cmd->BindStructuredBuffer(0, slot.GPU);

  const ::gecko::u32 w = info.Target.Desc.Width;
  const ::gecko::u32 h = info.Target.Desc.Height;
  cmd->SetViewport(0.0F, 0.0F, static_cast<::gecko::f32>(w),
                   static_cast<::gecko::f32>(h));
  cmd->SetScissor(0, 0, w, h);

  DebugLinePushConstants pc {
      .ViewportPx = {static_cast<::gecko::f32>(w),
                     static_cast<::gecko::f32>(h)},
      ._Pad = {0.0F, 0.0F},
  };
  cmd->SetConstants(
      0, ::gecko::Span<const ::gecko::byte> {
             reinterpret_cast<const ::gecko::byte*>(&pc), sizeof(pc)});

  // 6 vertices per line (2 triangles).
  cmd->Draw(batchSize * 6u, 1, m_BatchStart * 6u, 0);

  m_BatchStart = m_LineCursor;
}

void DebugRendererContext::EndFrame()
{
  if (!m_Valid)
    return;
  if (!m_FrameStarted)
    return;

  if (m_LineCursor > 0)
  {
    auto* device = ::gecko::graphics::GetGraphicsDevice();
    if (!device)
    {
      GECKO_ERROR(labels::Context, "GraphicsModule did not publish a device");
      return;
    }

    auto& slot = m_FrameSlots[m_CurrentSlot];
    const auto* raw = reinterpret_cast<const ::gecko::byte*>(slot.CPU.data());
    device->UploadBufferData(slot.GPU, {raw, sizeof(Line2D) * m_LineCursor});
  }

  m_FrameUploaded = true;
}

}  // namespace gecko::debug_renderer
