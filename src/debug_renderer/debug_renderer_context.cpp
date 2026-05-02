#include "gecko/debug_renderer/debug_renderer_context.h"

#include "gecko/core/services/log.h"
#include "gecko/debug_renderer/debug_renderer_module.h"
#include "gecko/graphics/graphics_module.h"
#include "gecko/graphics/graphics_types.h"
#include "private/labels.h"
#include "private/types.h"

namespace gecko::debug_renderer {

DebugRendererContext::DebugRendererContext()
{
  CreateRenderResources();
}

void DebugRendererContext::NewFrame()
{
  m_CurrentLineIndex = 0;
  m_CurrentTargetIndex = 0;
}

void DebugRendererContext::DrawLine(math::float2 a, math::float2 b,
                                    math::float3 color, f32 thickness)
{
  if (m_CurrentLineIndex >= m_LineBufferCPU.size())
  {
    GECKO_WARN(labels::DebugRenderer,
               "Exceeded debug line buffer capacity; some lines will not be "
               "rendered");
    return;
  }
  m_LineBufferCPU[m_CurrentLineIndex++] = Line2D {a, b, color, thickness};
}

void DebugRendererContext::SetTarget(const graphics::RenderTarget& target)
{
  if (m_CurrentTargetIndex >= m_Targets.size())
  {
    GECKO_WARN(
        labels::DebugRenderer,
        "Exceeded debug target capacity; some lines will not be rendered");
    return;
  }
  m_Targets[m_CurrentTargetIndex++] = Target {target, m_CurrentLineIndex, 0};
}

void DebugRendererContext::Submit(gecko::graphics::ICommandList* cmd)
{
  // Update line counts for each target based on the next target's line begin.
  for (u32 i = 0; i < m_CurrentTargetIndex; ++i)
  {
    u32 begin = m_Targets[i].LineBeginIndex;
    u32 end = (i + 1 < m_CurrentTargetIndex) ? m_Targets[i + 1].LineBeginIndex
                                             : m_CurrentLineIndex;
    m_Targets[i].LineCount = end - begin;
  }

  // Upload the entire line buffer every frame. This is wasteful, but simple.
  // In a real implementation we'd want to only upload the portion of the
  // buffer that's actually used, and ideally use a more efficient strategy than
  // a full buffer update.
  auto* device = ::gecko::graphics::GetGraphicsDevice();
  if (!device)
  {
    GECKO_ERROR(labels::DebugRenderer,
                "GraphicsModule did not publish a device");
    return;
  }
  const auto* raw =
      reinterpret_cast<const ::gecko::byte*>(m_LineBufferCPU.data());
  device->UploadBufferData(m_LineBufferGPU,
                           {raw, sizeof(Line2D) * m_CurrentLineIndex});

  // Issue draw calls for each target.
  cmd->BindPipeline(GetDebugLinePipeline());
  cmd->BindStructuredBuffer(0, m_LineBufferGPU);
  for (u32 i = 0; i < m_CurrentTargetIndex; ++i)
  {
    const Target& target = m_Targets[i];
    if (target.LineCount == 0)
      continue;  // Skip targets with no lines.

    graphics::ClearValue clear =
        graphics::ClearValue::RenderTarget(0.05F, 0.05F, 0.08F, 1.0F);
    cmd->BeginRendering(target.RenderTarget, &clear);
    cmd->SetViewport(
        0.0F, 0.0F, static_cast<::gecko::f32>(target.RenderTarget.Desc.Width),
        static_cast<::gecko::f32>(target.RenderTarget.Desc.Height));
    cmd->SetScissor(0, 0, target.RenderTarget.Desc.Width,
                    target.RenderTarget.Desc.Height);

    DebugLinePushConstants pc {
        .ViewportPx =
            {static_cast<::gecko::f32>(target.RenderTarget.Desc.Width),
             static_cast<::gecko::f32>(target.RenderTarget.Desc.Height)},
        ._Pad = {0.0f, 0.0f},
    };
    cmd->SetConstants(
        0, ::gecko::Span<const ::gecko::byte>(
               reinterpret_cast<const ::gecko::byte*>(&pc), sizeof(pc)));

    cmd->Draw(target.LineCount * 6u, 1, target.LineBeginIndex * 6u,
              0);  // 6 vertices per line (2 triangles)
  }
}

bool DebugRendererContext::CreateRenderResources()
{
  gecko::graphics::GraphicsDevice* device =
      ::gecko::graphics::GetGraphicsDevice();
  if (!device)
  {
    GECKO_ERROR(labels::DebugRenderer,
                "GraphicsModule did not publish a device");
    return false;
  }

  graphics::StructuredBufferDesc vbDesc;
  vbDesc.ElementSize = sizeof(Line2D);
  vbDesc.NumElements = static_cast<::gecko::u32>(m_LineBufferCPU.size());
  vbDesc.Memory = graphics::MemoryType::Dedicated;
  m_LineBufferGPU = device->CreateStructuredBuffer(vbDesc);
  if (!m_LineBufferGPU.IsValid())
  {
    GECKO_ERROR(labels::DebugRenderer, "Failed to create line buffer");
    return false;
  }
  const auto* raw =
      reinterpret_cast<const ::gecko::byte*>(m_LineBufferCPU.data());
  device->UploadBufferData(m_LineBufferGPU,
                           {raw, sizeof(Line2D) * m_LineBufferCPU.size()});
  return true;
}
}  // namespace gecko::debug_renderer