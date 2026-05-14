#include "gecko/debug_renderer/debug_renderer_context.h"

#include "gecko/core/services/log.h"
#include "gecko/graphics/graphics_module.h"
#include "gecko/math/vector.h"
#include "private/labels.h"
#include "private/types.h"

namespace gecko::debug_renderer {

DebugRendererContext::DebugRendererContext(::gecko::u32 lineCapacity, ::gecko::u32 framesInFlight)
{
  if (lineCapacity == 0)
  {
    GECKO_ERROR(labels::Context, "lineCapacity must be > 0");
    return;
  }
  if (framesInFlight == 0 || framesInFlight > MaxFramesInFlight)
  {
    GECKO_ERROR(labels::Context, "framesInFlight (%u) must be in [1, %u]", framesInFlight, MaxFramesInFlight);
    return;
  }

  auto* device = ::gecko::graphics::GetGraphicsDevice();
  if (!device)
  {
    GECKO_ERROR(labels::Context, "GraphicsModule did not publish a device");
    return;
  }

  m_FrameSlots.resize(framesInFlight);

  {
    m_LineCapacity = lineCapacity;
    ::gecko::graphics::StructuredBufferDesc desc {};
    desc.ElementSize = sizeof(Line2D);
    desc.NumElements = m_LineCapacity;
    desc.Memory = ::gecko::graphics::MemoryType::Dedicated;

    for (auto& slot : m_FrameSlots)
    {
      slot.CPU.resize(m_LineCapacity);
      slot.GPU = device->CreateStructuredBuffer(desc);
      if (!slot.GPU.IsValid())
      {
        GECKO_ERROR(labels::Context, "Failed to create per-frame line buffer");
        m_FrameSlots.clear();
        return;
      }
    }
  }

  {
    m_CharCapacity = 100;
    ::gecko::graphics::StructuredBufferDesc desc {};
    desc.ElementSize = sizeof(Char2D);
    desc.NumElements = m_CharCapacity;
    desc.Memory = ::gecko::graphics::MemoryType::Dedicated;

    for (auto& slot : m_FrameSlots)
    {
      slot.CPU_chars.resize(m_CharCapacity);
      slot.GPU_chars = device->CreateStructuredBuffer(desc);
      if (!slot.GPU_chars.IsValid())
      {
        GECKO_ERROR(labels::Context, "Failed to create per-frame char buffer");
        m_FrameSlots.clear();
        return;
      }
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
               "NewFrame called before EndFrame; previous frame's %u line(s) "
               "were never uploaded to the GPU and will not appear",
               m_LineCursor);
  }

  m_CurrentSlot = (m_CurrentSlot + 1) % m_FrameSlots.size();
  m_LineCursor = 0;
  m_BatchStart = 0;

  m_CharCursor = 0;
  m_CharBatchStart = 0;

  m_CurrentTarget = {};
  m_FrameStarted = true;
  m_FrameUploaded = false;
  m_FrameBound = false;
  m_LineOverflowWarned = false;
  m_CharOverflowWarned = false;
}

void DebugRendererContext::SetFrame(::gecko::graphics::RenderTarget target)
{
  if (!m_Valid)
    return;
  if (!m_FrameStarted)
  {
    GECKO_WARN(labels::Context, "SetFrame called before NewFrame; ignored");
    return;
  }
  m_CurrentTarget = ::std::move(target);
  m_FrameBound = true;
}

void DebugRendererContext::DrawLine(::gecko::math::float2 a, ::gecko::math::float2 b, ::gecko::math::float3 color,
                                    ::gecko::f32 thickness)
{
  if (!m_Valid)
    return;
  if (!m_FrameStarted)
    return;

  if (m_LineCursor >= m_LineCapacity)
  {
    if (!m_LineOverflowWarned)
    {
      GECKO_WARN(labels::Context,
                 "Exceeded debug line buffer capacity (%u); subsequent lines "
                 "this frame will be dropped",
                 m_LineCapacity);
      m_LineOverflowWarned = true;
    }
    return;
  }

  auto& slot = m_FrameSlots[m_CurrentSlot];
  slot.CPU[m_LineCursor++] = Line2D {a, b, color, thickness};
}

void DebugRendererContext::DrawText(const char* text, ::gecko::math::float2 pos,  math::Float3 color, f32 scale)
{
  if (!m_Valid)
    return;
  if (!m_FrameStarted)
    return;

  u32 idx = 0;
  char c = text[idx];
  math::float2 c_pos = pos;
  while (c)
  {
    if (m_CharCursor >= m_CharCapacity)
    {
      if (!m_CharOverflowWarned)
      {
        GECKO_WARN(labels::Context,
                   "Exceeded debug line buffer capacity (%u); subsequent lines "
                   "this frame will be dropped",
                   m_CharCapacity);
        m_CharOverflowWarned = true;
      }
      return;
    }

    auto& slot = m_FrameSlots[m_CurrentSlot];
    slot.CPU_chars[m_CharCursor++] = Char2D{.Position = c_pos, .size = scale, ._Pad = 0, .Color = color, ._Pad2 = 0};
    c_pos.X += scale;
    c = text[++idx];
  }
}

void DebugRendererContext::Submit(::gecko::graphics::ICommandList* cmd)
{
  if (!cmd || !m_Valid)
    return;
  if (!m_FrameStarted)
  {
    GECKO_WARN(labels::Context, "Submit called before NewFrame; ignored");
    return;
  }
  if (!m_FrameBound)
  {
    GECKO_WARN(labels::Context, "Submit called before SetFrame; ignored");
    return;
  }

  const ::gecko::u32 w = m_CurrentTarget.Desc.Width;
  const ::gecko::u32 h = m_CurrentTarget.Desc.Height;
  cmd->SetViewport(0.0F, 0.0F, static_cast<::gecko::f32>(w), static_cast<::gecko::f32>(h));
  cmd->SetScissor(0, 0, w, h);

  auto& slot = m_FrameSlots[m_CurrentSlot];

  {
    const ::gecko::u32 batchSize = m_LineCursor - m_BatchStart;
    if (batchSize == 0)
      return;

    cmd->BindPipeline(GetDebugLinePipeline());
    cmd->BindStructuredBuffer(0, slot.GPU);

    DebugLinePushConstants pc {
        .ViewportPx = {static_cast<::gecko::f32>(w), static_cast<::gecko::f32>(h)},
        ._Pad = {0.0F, 0.0F},
    };
    cmd->SetConstants(0, ::gecko::Span<const ::gecko::byte> {reinterpret_cast<const ::gecko::byte*>(&pc), sizeof(pc)});

    // 6 vertices per line (2 triangles).
    cmd->Draw(batchSize * 6u, 1, m_BatchStart * 6u, 0);
    m_BatchStart = m_LineCursor;
  }

  {
    const ::gecko::u32 batchSize = m_CharCursor - m_CharBatchStart;
    if (batchSize == 0)
      return;

    cmd->BindPipeline(GetDebugTextPipeline());
    cmd->BindStructuredBuffer(0, slot.GPU_chars);

    DebugLinePushConstants pc {
        .ViewportPx = {static_cast<::gecko::f32>(w), static_cast<::gecko::f32>(h)},
        ._Pad = {0.0F, 0.0F},
    };
    cmd->SetConstants(0, ::gecko::Span<const ::gecko::byte> {reinterpret_cast<const ::gecko::byte*>(&pc), sizeof(pc)});

    // 6 vertices per line (2 triangles).
    cmd->Draw(batchSize * 6u, 1, m_CharBatchStart * 6u, 0);
    m_CharBatchStart = m_CharCursor;
  }

}

void DebugRendererContext::EndFrame()
{
  if (!m_Valid)
    return;
  if (!m_FrameStarted)
    return;

  if (m_LineCursor > m_BatchStart)
  {
    GECKO_WARN(labels::Context,
               "EndFrame: %u line(s) appended after the last Submit were not "
               "drawn this frame",
               m_LineCursor - m_BatchStart);
  }

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

  if (m_CharCursor > m_CharBatchStart)
  {
    GECKO_WARN(labels::Context,
               "EndFrame: %u char(s) appended after the last Submit were not "
               "drawn this frame",
               m_CharCursor - m_CharBatchStart);
  }

  if (m_CharCursor > 0)
  {
    auto* device = ::gecko::graphics::GetGraphicsDevice();
    if (!device)
    {
      GECKO_ERROR(labels::Context, "GraphicsModule did not publish a device");
      return;
    }

    auto& slot = m_FrameSlots[m_CurrentSlot];
    const auto* raw = reinterpret_cast<const ::gecko::byte*>(slot.CPU_chars.data());
    device->UploadBufferData(slot.GPU_chars, {raw, sizeof(Char2D) * m_CharCursor});
  }
  m_FrameUploaded = true;
}

}  // namespace gecko::debug_renderer
