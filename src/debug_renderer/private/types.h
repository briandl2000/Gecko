#include "gecko/core/types.h"
#include "gecko/math/math.h"

#include <gecko/graphics/graphics_types.h>

namespace gecko::debug_renderer {

struct DebugLinePushConstants
{
  ::gecko::math::float2 ViewportPx;
  ::gecko::math::float2 _Pad;
};

const gecko::graphics::GraphicsPipeline& GetDebugLinePipeline();

}  // namespace gecko::debug_renderer