#include "gecko/debug_renderer/debug_renderer.h"

#include "debug_renderer/Shaders.generated.h"
#include "gecko/core/containers/array.h"
#include "gecko/core/services/log.h"
#include "gecko/graphics/graphics.h"

namespace gecko::debug_renderer {
namespace {

constexpr Label DebugRendererLabel = MakeLabel("debug_renderer");

struct Line
{
  math::Float2 A;
  math::Float2 B;
  math::Float3 Color;
  f32 Thickness;
};

struct PushConstants
{
  math::Float2 ViewportPx;
  math::Float2 Padding;
};

struct State
{
  Array<Line> Lines;
  graphics::Buffer LineBuffer;
  graphics::GraphicsPipeline Pipeline;
  u32 Capacity {0};
  bool OverflowReported {false};
};

State g_State;

graphics::ShaderCode ShaderCode(const u32* words, usize size) noexcept
{
  return {
      .Format = graphics::ShaderFormat::SPIRV,
      .Bytes = {reinterpret_cast<const byte*>(words), size},
  };
}

}  // namespace

bool Initialize(graphics::DataFormat renderTargetFormat, u32 lineCapacity) noexcept
{
  if (IsInitialized())
    return true;
  auto* device = graphics::GetGraphicsDevice();
  if (device == nullptr || lineCapacity == 0 || renderTargetFormat == graphics::DataFormat::None)
    return false;

  graphics::StructuredBufferDesc bufferDesc {
      .NumElements = lineCapacity,
      .ElementSize = sizeof(Line),
      .Memory = graphics::MemoryType::Dedicated,
      .DebugName = "Debug renderer lines",
  };
  g_State.LineBuffer = device->CreateStructuredBuffer(bufferDesc);
  if (!g_State.LineBuffer.IsValid())
    return false;

  graphics::GraphicsPipelineDesc pipelineDesc {};
  pipelineDesc.VertexShader = ShaderCode(shaders::DebugLineVertex, sizeof(shaders::DebugLineVertex));
  pipelineDesc.PixelShader = ShaderCode(shaders::DebugLinePixel, sizeof(shaders::DebugLinePixel));
  pipelineDesc.NumRenderTargets = 1;
  pipelineDesc.RenderTargetFormats[0] = renderTargetFormat;
  pipelineDesc.PipelineResources[0] =
      graphics::PipelineResource::StructuredBufferBinding(1, graphics::ShaderType::Vertex);
  pipelineDesc.NumPipelineResources = 1;
  pipelineDesc.PushConstantBytes = sizeof(PushConstants);
  pipelineDesc.DebugName = "Debug renderer lines";
  g_State.Pipeline = device->CreateGraphicsPipeline(pipelineDesc);
  if (!g_State.Pipeline.IsValid())
  {
    g_State.LineBuffer = {};
    return false;
  }

  g_State.Lines.Reserve(lineCapacity);
  g_State.Capacity = lineCapacity;
  return true;
}

void Shutdown() noexcept
{
  g_State.Lines.Clear();
  g_State.Pipeline = {};
  g_State.LineBuffer = {};
  g_State.Capacity = 0;
  g_State.OverflowReported = false;
}

bool IsInitialized() noexcept
{
  return g_State.Capacity != 0 && g_State.LineBuffer.IsValid() && g_State.Pipeline.IsValid();
}

void BeginFrame() noexcept
{
  g_State.Lines.Clear();
  g_State.OverflowReported = false;
}

void DrawLine(math::Float2 a, math::Float2 b, math::Float3 color, f32 thickness) noexcept
{
  if (!IsInitialized())
    return;
  if (g_State.Lines.Count() >= g_State.Capacity)
  {
    if (!g_State.OverflowReported)
    {
      GECKO_WARN(DebugRendererLabel, "Line capacity reached: {}", g_State.Capacity);
      g_State.OverflowReported = true;
    }
    return;
  }
  g_State.Lines.PushBack({a, b, color, thickness});
}

void Submit(graphics::ICommandList& commandList, const graphics::RenderTarget& target) noexcept
{
  if (!IsInitialized() || g_State.Lines.Empty() || !target.IsValid())
    return;

  auto* device = graphics::GetGraphicsDevice();
  if (device == nullptr)
    return;
  const auto* data = reinterpret_cast<const byte*>(g_State.Lines.Data());
  device->UploadBufferData(g_State.LineBuffer, {data, sizeof(Line) * g_State.Lines.Count()});

  PushConstants constants {
      .ViewportPx = {static_cast<f32>(target.Desc.Width), static_cast<f32>(target.Desc.Height)},
      .Padding = {},
  };
  commandList.SetViewport(0.0F, 0.0F, constants.ViewportPx.X, constants.ViewportPx.Y);
  commandList.SetScissor(0, 0, target.Desc.Width, target.Desc.Height);
  commandList.BindPipeline(g_State.Pipeline);
  commandList.BindStructuredBuffer(0, g_State.LineBuffer);
  commandList.SetConstants(0, {reinterpret_cast<const byte*>(&constants), sizeof(constants)});
  commandList.Draw(static_cast<u32>(g_State.Lines.Count()) * 6U);
}

}  // namespace gecko::debug_renderer
