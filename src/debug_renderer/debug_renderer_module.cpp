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

namespace {

::gecko::graphics::GraphicsPipeline g_DebugLinePipeline {};

constexpr ::gecko::ServiceId RequiredServices[] = {
    ::gecko::ServiceIdOf<::gecko::graphics::GraphicsDevice>(),
};

bool CreatePipeline()
{
  auto* device = ::gecko::graphics::GetGraphicsDevice();
  if (!device)
  {
    GECKO_ERROR(labels::Pipeline, "GraphicsModule did not publish a device");
    return false;
  }

  ::gecko::graphics::GraphicsPipelineDesc desc {};
  desc.VertexShader = ::gecko::graphics::ShaderCode {
      .Format = ::gecko::graphics::ShaderFormat::SPIRV,
      .Bytes = {reinterpret_cast<const ::gecko::byte*>(shaders::DebugLineVert),
                sizeof(shaders::DebugLineVert)},
  };
  desc.PixelShader = ::gecko::graphics::ShaderCode {
      .Format = ::gecko::graphics::ShaderFormat::SPIRV,
      .Bytes = {reinterpret_cast<const ::gecko::byte*>(shaders::DebugLineFrag),
                sizeof(shaders::DebugLineFrag)},
  };
  desc.PipelineResources[0] =
      ::gecko::graphics::PipelineResource::StructuredBufferBinding(
          1, ::gecko::graphics::ShaderType::Vertex);
  desc.NumPipelineResources = 1;
  desc.PushConstantBytes = sizeof(DebugLinePushConstants);
  desc.NumRenderTargets = 1;
  desc.RenderTargetFormats[0] = ::gecko::graphics::DataFormat::R8G8B8A8_UNORM;
  desc.Culling = ::gecko::graphics::CullMode::None;
  desc.DebugName = "DebugLinePipeline";

  g_DebugLinePipeline = device->CreateGraphicsPipeline(desc);
  if (!g_DebugLinePipeline.IsValid())
  {
    GECKO_ERROR(labels::Pipeline, "Failed to create debug line pipeline");
    return false;
  }
  return true;
}

}  // namespace

const ::gecko::graphics::GraphicsPipeline& GetDebugLinePipeline()
{
  return g_DebugLinePipeline;
}

::gecko::Span<const ::gecko::ServiceId> DebugRendererModule::Requires()
    const noexcept
{
  return ::gecko::Span<const ::gecko::ServiceId> {RequiredServices};
}

bool DebugRendererModule::Startup(
    ::gecko::IModuleRegistry& /*modules*/) noexcept
{
  GECKO_SCOPE(labels::DebugRenderer);
  return CreatePipeline();
}

void DebugRendererModule::Shutdown(
    ::gecko::IModuleRegistry& /*modules*/) noexcept
{
  GECKO_SCOPE(labels::DebugRenderer);
  g_DebugLinePipeline = {};
}

}  // namespace gecko::debug_renderer
