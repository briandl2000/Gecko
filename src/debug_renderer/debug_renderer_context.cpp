#include "gecko/debug_renderer/debug_renderer_context.h"

#include "gecko/core/services/log.h"
#include "gecko/debug_renderer/debug_renderer_module.h"
#include "gecko/graphics/graphics_module.h"
#include "gecko/graphics/graphics_types.h"
#include "private/labels.h"
#include "private/types.h"

namespace gecko::debug_renderer {

DebugRendererContext::DebugRendererContext(::gecko::u32 lineCapacity)
{
  if (lineCapacity == 0)
  {
    GECKO_ERROR(labels::Context,
                "DebugRendererContext: lineCapacity must be > 0");
    return;
  }

  auto* device = ::gecko::graphics::GetGraphicsDevice();
  if (!device)
  {
    GECKO_ERROR(labels::Context, "GraphicsModule did not publish a device");
    return;
  }

  m_LineBufferCPU.resize(lineCapacity);

  ::gecko::graphics::StructuredBufferDesc desc {};
  desc.ElementSize = sizeof(Line2D);
  desc.NumElements = lineCapacity;
  desc.Memory = ::gecko::graphics::MemoryType::Dedicated;
  m_LineBufferGPU = device->CreateStructuredBuffer(desc);
  if (!m_LineBufferGPU.IsValid())
  {
    GECKO_ERROR(labels::Context, "Failed to create line buffer");
    m_LineBufferCPU.clear();
    m_LineBufferCPU.shrink_to_fit();
    return;
  }
}

void DebugRendererContext::NewFrame()
{
  m_CurrentLineIndex = 0;
  m_CurrentTargetIndex = 0;
  m_LineOverflowWarned = false;
  m_TargetOverflowWarned = false;
}

void DebugRendererContext::SetTarget(
    const ::gecko::graphics::RenderTarget& target,
    const ::gecko::graphics::ClearValue* clear)
{
  if (m_CurrentTargetIndex >= m_Targets.size())
  {
    if (!m_TargetOverflowWarned)
    {
      GECKO_WARN(labels::Context,
                 "Exceeded debug target capacity ({}); subsequent targets "
                 "this frame will be dropped",
                 static_cast<::gecko::u32>(m_Targets.size()));
      m_TargetOverflowWarned = true;
    }
    return;
  }
  Target& slot = m_Targets[m_CurrentTargetIndex++];
  slot.RenderTarget = target;
  slot.LineBeginIndex = m_CurrentLineIndex;
  slot.LineCount = 0;
  slot.HasClear = (clear != nullptr);
  slot.Clear = clear ? *clear : ::gecko::graphics::ClearValue {};
}

void DebugRendererContext::DrawLine(::gecko::math::float2 a,
                                    ::gecko::math::float2 b,
                                    ::gecko::math::float3 color,
                                    ::gecko::f32 thickness)
{
  if (m_CurrentLineIndex >= m_LineBufferCPU.size())
  {
    if (!m_LineOverflowWarned)
    {
      GECKO_WARN(labels::Context,
                 "Exceeded debug line buffer capacity ({}); subsequent lines "
                 "this frame will be dropped",
                 static_cast<::gecko::u32>(m_LineBufferCPU.size()));
      m_LineOverflowWarned = true;
    }
    return;
  }
  m_LineBufferCPU[m_CurrentLineIndex++] = Line2D {a, b, color, thickness};
}

void DebugRendererContext::Submit(::gecko::graphics::ICommandList* cmd)
{
  if (!cmd || !IsValid() || m_CurrentTargetIndex == 0)
    return;

  // Resolve per-target line counts from the recorded begin indices.
  for (::gecko::u32 i = 0; i < m_CurrentTargetIndex; ++i)
  {
    const ::gecko::u32 begin = m_Targets[i].LineBeginIndex;
    const ::gecko::u32 end = (i + 1 < m_CurrentTargetIndex)
                                 ? m_Targets[i + 1].LineBeginIndex
                                 : m_CurrentLineIndex;
    m_Targets[i].LineCount = end - begin;
  }

  auto* device = ::gecko::graphics::GetGraphicsDevice();
  if (!device)
  {
    GECKO_ERROR(labels::Context, "GraphicsModule did not publish a device");
    return;
  }

  // Upload only the lines that were actually recorded this frame.
  if (m_CurrentLineIndex > 0)
  {
    const auto* raw =
        reinterpret_cast<const ::gecko::byte*>(m_LineBufferCPU.data());
    device->UploadBufferData(m_LineBufferGPU,
                             {raw, sizeof(Line2D) * m_CurrentLineIndex});
  }

  cmd->BindPipeline(GetDebugLinePipeline());
  cmd->BindStructuredBuffer(0, m_LineBufferGPU);

  for (::gecko::u32 i = 0; i < m_CurrentTargetIndex; ++i)
  {
    const Target& target = m_Targets[i];
    if (target.LineCount == 0)
      continue;

    const ::gecko::graphics::ClearValue* clearPtr =
        target.HasClear ? &target.Clear : nullptr;
    cmd->BeginRendering(target.RenderTarget, clearPtr);
    cmd->SetViewport(
        0.0F, 0.0F, static_cast<::gecko::f32>(target.RenderTarget.Desc.Width),
        static_cast<::gecko::f32>(target.RenderTarget.Desc.Height));
    cmd->SetScissor(0, 0, target.RenderTarget.Desc.Width,
                    target.RenderTarget.Desc.Height);

    DebugLinePushConstants pc {
        .ViewportPx =
            {static_cast<::gecko::f32>(target.RenderTarget.Desc.Width),
             static_cast<::gecko::f32>(target.RenderTarget.Desc.Height)},
        ._Pad = {0.0F, 0.0F},
    };
    cmd->SetConstants(
        0, ::gecko::Span<const ::gecko::byte> {
               reinterpret_cast<const ::gecko::byte*>(&pc), sizeof(pc)});

    // 6 vertices per line (2 triangles).
    cmd->Draw(target.LineCount * 6u, 1, target.LineBeginIndex * 6u, 0);
    cmd->EndRendering();
  }
}

}  // namespace gecko::debug_renderer
