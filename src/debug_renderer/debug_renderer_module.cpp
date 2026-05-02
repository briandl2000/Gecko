#include "gecko/debug_renderer/debug_renderer_module.h"

#include "gecko/core/scope.h"
#include "gecko/core/services/log.h"
#include "gecko/graphics/graphics_device.h"
#include "gecko/graphics/graphics_module.h"
#include "gecko/graphics/graphics_types.h"
#include "private/labels.h"
#include "private/types.h"
#include "shaders.h"

namespace gecko::debug_renderer {

static ::gecko::graphics::GraphicsPipeline g_DebugLinePipeline {};

constexpr ::gecko::ServiceId RequiredServices[] = {
    ::gecko::ServiceIdOf<::gecko::graphics::GraphicsDevice>(),
};

bool CreatePipeline()
{
  auto device = ::gecko::graphics::GetGraphicsDevice();
  if (!device)
  {
    GECKO_ERROR(labels::DebugRenderer,
                "GraphicsModule did not publish a device");
    return false;
  }

  gecko::graphics::GraphicsPipelineDesc pDesc;
  pDesc.VertexShader = gecko::graphics::ShaderCode {
      .Format = gecko::graphics::ShaderFormat::SPIRV,
      .Bytes = {reinterpret_cast<const ::gecko::byte*>(shaders::DebugLineVert),
                sizeof(shaders::DebugLineVert)},
  };
  pDesc.PixelShader = gecko::graphics::ShaderCode {
      .Format = gecko::graphics::ShaderFormat::SPIRV,
      .Bytes = {reinterpret_cast<const ::gecko::byte*>(shaders::DebugLineFrag),
                sizeof(shaders::DebugLineFrag)},
  };
  pDesc.PipelineResources[0] =
      gecko::graphics::PipelineResource::StructuredBufferBinding(
          1, gecko::graphics::ShaderType::Vertex);
  pDesc.NumPipelineResources = 1;
  pDesc.PushConstantBytes = sizeof(DebugLinePushConstants);
  pDesc.NumRenderTargets = 1;
  pDesc.RenderTargetFormats[0] = gecko::graphics::DataFormat::R8G8B8A8_UNORM;
  pDesc.Culling = graphics::CullMode::None;
  pDesc.DebugName = "DebugLinePipeline";
  g_DebugLinePipeline = device->CreateGraphicsPipeline(pDesc);

  if (!g_DebugLinePipeline.IsValid())
  {
    GECKO_ERROR(labels::DebugRenderer, "Failed to create debug line pipeline");
    return false;
  }
  return true;
}

::gecko::Span<const ::gecko::ServiceId> DebugRendererModule::Requires()
    const noexcept
{
  return ::gecko::Span<const ::gecko::ServiceId> {RequiredServices};
}

const gecko::graphics::GraphicsPipeline& GetDebugLinePipeline()
{
  return g_DebugLinePipeline;
}

bool DebugRendererModule::Startup(
    ::gecko::IModuleRegistry& /*modules*/) noexcept
{
  GECKO_SCOPE(labels::DebugRenderer);

  if (!CreatePipeline())
    return false;

  return true;
}

void DebugRendererModule::Shutdown(
    ::gecko::IModuleRegistry& /*modules*/) noexcept
{
  GECKO_SCOPE(labels::DebugRenderer);
  g_DebugLinePipeline = {};
}

}  // namespace gecko::debug_renderer
