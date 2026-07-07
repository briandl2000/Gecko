#pragma once

#include "gecko/core/types.h"
#include "gecko/graphics/graphics_types.h"
#include "gecko/math/math.h"

namespace gecko::debug_renderer {

struct DebugLinePushConstants
{
  ::gecko::math::float2 ViewportPx;
  ::gecko::math::float2 _Pad;
};

struct GlyphData
{
  uint GlyphWidth;
  uint GlyphHeight;
  uint NumberOfGlyphsPerRow;
  uint NumberOfGlyphsPerColumn;
};

const ::gecko::graphics::GraphicsPipeline& GetDebugLinePipeline();
const ::gecko::graphics::GraphicsPipeline& GetDebugTextPipeline();
const graphics::Texture& GetGlyphTexture();
const graphics::Sampler& GetGlyphSampler();
const graphics::Buffer& GetGlyphDataBuffer();

}  // namespace gecko::debug_renderer
