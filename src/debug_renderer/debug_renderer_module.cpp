#include "gecko/debug_renderer/debug_renderer_module.h"

#include "gecko/core/scope.h"
#include "gecko/core/services/log.h"
#include "gecko/core/types.h"
#include "gecko/graphics/graphics_device.h"
#include "gecko/graphics/graphics_module.h"
#include "gecko/graphics/graphics_types.h"
#include "private/labels.h"
#include "private/types.h"
#include "shaders.h"
#include "private/debug_text_glyphs.h"

#include <vector>

namespace gecko::debug_renderer {

namespace {

::gecko::graphics::GraphicsPipeline g_DebugLinePipeline {};
::gecko::graphics::GraphicsPipeline g_DebugTextPipeline {};
graphics::Texture g_GlyphTexture = {};
graphics::Sampler g_GlyphSampler = {};
graphics::Buffer g_GlyphDataBuffer = {};

constexpr ::gecko::ServiceId RequiredServices[] = {
    ::gecko::ServiceIdOf<::gecko::graphics::GraphicsDevice>(),
};

bool CreatePipelines(graphics::GraphicsDevice* device)
{
  // Line pipeline
  {
    ::gecko::graphics::GraphicsPipelineDesc desc {};
    desc.VertexShader = ::gecko::graphics::ShaderCode {
        .Format = ::gecko::graphics::ShaderFormat::SPIRV,
        .Bytes = {reinterpret_cast<const ::gecko::byte*>(shaders::DebugLineVert), sizeof(shaders::DebugLineVert)},
    };
    desc.PixelShader = ::gecko::graphics::ShaderCode {
        .Format = ::gecko::graphics::ShaderFormat::SPIRV,
        .Bytes = {reinterpret_cast<const ::gecko::byte*>(shaders::DebugLineFrag), sizeof(shaders::DebugLineFrag)},
    };
    desc.PipelineResources[0] =
        ::gecko::graphics::PipelineResource::StructuredBufferBinding(1, ::gecko::graphics::ShaderType::Vertex);
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
  }

  {
    ::gecko::graphics::GraphicsPipelineDesc desc {};
    desc.VertexShader = ::gecko::graphics::ShaderCode {
        .Format = ::gecko::graphics::ShaderFormat::SPIRV,
        .Bytes = {reinterpret_cast<const ::gecko::byte*>(shaders::DebugTextVert), sizeof(shaders::DebugTextVert)},
    };
    desc.PixelShader = ::gecko::graphics::ShaderCode {
        .Format = ::gecko::graphics::ShaderFormat::SPIRV,
        .Bytes = {reinterpret_cast<const ::gecko::byte*>(shaders::DebugTextFrag), sizeof(shaders::DebugTextFrag)},
    };
    desc.PipelineResources[0] =
        ::gecko::graphics::PipelineResource::StructuredBufferBinding(1, ::gecko::graphics::ShaderType::Vertex);
    desc.PipelineResources[1] = graphics::PipelineResource::ConstantBufferBinding(1, graphics::ShaderType::Vertex);
    desc.PipelineResources[2] = graphics::PipelineResource::TextureBinding(1, graphics::ShaderType::Pixel);
    desc.PipelineResources[3] = graphics::PipelineResource::SamplerBinding(1, graphics::ShaderType::Pixel);
    desc.NumPipelineResources = 4;
    desc.PushConstantBytes = sizeof(DebugLinePushConstants);
    desc.NumRenderTargets = 1;
    desc.RenderTargetFormats[0] = ::gecko::graphics::DataFormat::R8G8B8A8_UNORM;
    desc.Culling = ::gecko::graphics::CullMode::None;
    desc.DebugName = "DebugTextPipeline";

    g_DebugTextPipeline = device->CreateGraphicsPipeline(desc);
    if (!g_DebugTextPipeline.IsValid())
    {
      GECKO_ERROR(labels::Pipeline, "Failed to create debug text pipeline");
      return false;
    }
  }

  return true;
}

bool GenerateGlyphTable(graphics::GraphicsDevice* device)
{
  GlyphData glyphData;
  glyphData.GlyphWidth = DebugTextGlyphWidth;
  glyphData.GlyphHeight = DebugTextGlyphHeight;
  glyphData.NumberOfGlyphsPerRow = 16;
  glyphData.NumberOfGlyphsPerColumn = 8;

  graphics::ConstantBufferDesc glyphDataDesc;
  glyphDataDesc.SizeInBytes = sizeof(GlyphData);
  glyphDataDesc.Memory = graphics::MemoryType::Dedicated;

  g_GlyphDataBuffer = device->CreateConstantBuffer(glyphDataDesc);
  if (!g_GlyphDataBuffer.IsValid()) {
    GECKO_ERROR(labels::DebugRenderer, "Failed to create glyph data buffer.");
    return false;
  }

  Span<byte> glyphDataSpan = {reinterpret_cast<byte*>(&glyphData), glyphDataDesc.SizeInBytes};
  device->UploadBufferData(g_GlyphDataBuffer, glyphDataSpan);


  graphics::TextureDesc glyphTextureDesc;

  glyphTextureDesc.Format = graphics::DataFormat::R8_UNORM;
  glyphTextureDesc.Width = glyphData.GlyphWidth * glyphData.NumberOfGlyphsPerRow;
  glyphTextureDesc.Height = glyphData.GlyphHeight * glyphData.NumberOfGlyphsPerColumn;
  glyphTextureDesc.Type = graphics::TextureType::Tex2D;
  glyphTextureDesc.Memory = graphics::MemoryType::Dedicated;

  g_GlyphTexture = device->CreateTexture(glyphTextureDesc);
  if (!g_GlyphTexture.IsValid()) {
    GECKO_ERROR(labels::DebugRenderer, "Failed to create glyph texture.");
    return false;
  }

  std::vector<u8> glyphPixels = {};
  glyphPixels.resize(static_cast<usize>(glyphTextureDesc.Width * glyphTextureDesc.Height), 0);

  for (u32 x = 0; x < glyphData.NumberOfGlyphsPerRow; x++)
  {
    for (u32 y = 0; y < glyphData.NumberOfGlyphsPerColumn; y++)
    {
      u32 glyphIndex = x + y * glyphData.NumberOfGlyphsPerRow;
      DebugTextGlyph glyph = DebugTextGlyphs[glyphIndex];
      for (u32 j = 0; j < glyphData.GlyphHeight; j++)
      {
        u32 x_start = x * glyphData.GlyphWidth;
        u32 y_start = y * glyphData.GlyphHeight;
        u8 bit_row = glyph.bitmap[j];
        for (u32 i = 0; i < glyphData.GlyphWidth; i++)
        {
          u32 pixelIndex = (x_start + (i)) + (y_start + j) * glyphTextureDesc.Width;
          u8 pixelValue = bit_row & (1 << (7 - 1 - i));
          glyphPixels[pixelIndex] = pixelValue * 255;
        }
      }
    }
  }

  Span<byte> data = {reinterpret_cast<byte*>(glyphPixels.data()), glyphPixels.size() * sizeof(u8)};

  device->UploadTextureData(g_GlyphTexture, data);

  graphics::SamplerDesc samplerDesc {};
  samplerDesc.Filter = graphics::SamplerFilter::Point;
  samplerDesc.WrapMode = graphics::SamplerWrapMode::Wrap;
  g_GlyphSampler = device->CreateSampler(samplerDesc);
  if (!g_GlyphSampler.IsValid()) {
    GECKO_ERROR(labels::DebugRenderer, "Failed to create glyph sampler.");
    return false;
  }

  return true;
}

}  // namespace

const ::gecko::graphics::GraphicsPipeline& GetDebugLinePipeline()
{
  return g_DebugLinePipeline;
}

const ::gecko::graphics::GraphicsPipeline& GetDebugTextPipeline()
{
  return g_DebugTextPipeline;
}

const graphics::Texture& GetGlyphTexture()
{
  return g_GlyphTexture;
}

const graphics::Sampler& GetGlyphSampler()
{
  return g_GlyphSampler;
}

const graphics::Buffer& GetGlyphDataBuffer()
{
  return g_GlyphDataBuffer;
}

::gecko::Span<const ::gecko::ServiceId> DebugRendererModule::Requires() const noexcept
{
  return ::gecko::Span<const ::gecko::ServiceId> {RequiredServices};
}

bool DebugRendererModule::Startup(::gecko::IModuleRegistry& /*modules*/) noexcept
{
  GECKO_SCOPE(labels::DebugRenderer);

  auto* device = ::gecko::graphics::GetGraphicsDevice();
  if (!device)
  {
    GECKO_ERROR(labels::Pipeline, "GraphicsModule did not publish a device");
    return false;
  }

  bool success = CreatePipelines(device);
  success &= GenerateGlyphTable(device);
  return success;
}

void DebugRendererModule::Shutdown(::gecko::IModuleRegistry& /*modules*/) noexcept
{
  GECKO_SCOPE(labels::DebugRenderer);
  g_GlyphSampler = {};
  g_GlyphTexture = {};
  g_GlyphDataBuffer = {};
  g_DebugLinePipeline = {};
  g_DebugTextPipeline = {};
}

}  // namespace gecko::debug_renderer
