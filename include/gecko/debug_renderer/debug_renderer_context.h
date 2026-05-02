#pragma once

#include "gecko/math/math.h"

#include <array>
#include <gecko/core/types.h>
#include <gecko/graphics/command_list.h>
#include <gecko/graphics/graphics_types.h>

namespace gecko::debug_renderer {

class DebugRendererContext
{
public:
  DebugRendererContext();
  ~DebugRendererContext() = default;

  void NewFrame();
  void DrawLine(math::float2 a, math::float2 b, math::float3 color,
                f32 thickness);
  void SetTarget(const graphics::RenderTarget& target);
  void Submit(gecko::graphics::ICommandList* cmd);

private:
  bool CreateRenderResources();

  struct Line2D
  {
    math::float2 A;      ///< Start, pixels, top-left origin.
    math::float2 B;      ///< End,   pixels.
    math::float3 Color;  ///< RGB, linear, [0,1].
    f32 Thickness;       ///< Width in pixels.
  };

  std::array<Line2D, 1024 * 1024> m_LineBufferCPU {};
  u32 m_CurrentLineIndex {0};

  ::gecko::graphics::Buffer m_LineBufferGPU {};

  struct Target
  {
    graphics::RenderTarget RenderTarget {};
    u32 LineBeginIndex {0};
    u32 LineCount {0};
  };
  // We support up to 8 simultaneous targets; if we exceed this, some lines
  // will not be rendered, but we won't crash or overwrite memory.
  std::array<Target, 8> m_Targets {};
  u32 m_CurrentTargetIndex {0};
};

}  // namespace gecko::debug_renderer