#pragma once

/// @file
/// `DebugRendererContext` -- per-view recorder of 2D debug lines.
/// Lines accumulated during a frame are uploaded and drawn by
/// `Submit`. One context owns one CPU/GPU line buffer; create more
/// contexts to render different content into different surfaces.

#include <array>
#include <gecko/core/types.h>
#include <gecko/graphics/command_list.h>
#include <gecko/graphics/graphics_types.h>
#include <gecko/math/math.h>
#include <vector>

namespace gecko::debug_renderer {

/// Records 2D debug lines for one frame and submits them to a graphics
/// command list. One context per logical "view" -- a single context
/// can fan recorded lines out to several render targets via
/// `SetTarget`.
///
/// Lifetime: the context owns a GPU buffer; it MUST be destroyed
/// before the `GraphicsDevice` (i.e. before `GraphicsModule` shuts
/// down).
///
/// Frame loop:
/// @code
///   ctx.NewFrame();
///   ctx.SetTarget(backBuffer);  // optionally with a clear value
///   ctx.DrawLine(a, b, color, thickness);
///   ...
///   ctx.Submit(cmd);            // begins/ends a render pass per target
/// @endcode
class DebugRendererContext
{
public:
  /// Maximum number of distinct render targets a single frame can fan
  /// recorded lines out to. Excess `SetTarget` calls in one frame are
  /// dropped with a warning rather than overrunning memory.
  static constexpr ::gecko::u32 MaxTargetsPerFrame = 8;

  /// Default capacity of the per-context line buffer used when no
  /// explicit capacity is supplied to the constructor. 128K lines is
  /// roughly 4 MB of GPU memory at `sizeof(Line2D) == 32`.
  static constexpr ::gecko::u32 DefaultLineCapacity = 128u * 1024u;

  /// Construct a context with at most `lineCapacity` lines recordable
  /// per frame. Allocates a CPU staging buffer and a matching GPU
  /// structured buffer up-front. Must be called after
  /// `GraphicsModule::Startup`.
  explicit DebugRendererContext(
      ::gecko::u32 lineCapacity = DefaultLineCapacity);
  ~DebugRendererContext() = default;

  DebugRendererContext(const DebugRendererContext&) = delete;
  DebugRendererContext& operator=(const DebugRendererContext&) = delete;

  /// Returns true if both the CPU staging buffer and the GPU
  /// structured buffer were created successfully.
  [[nodiscard]] bool IsValid() const noexcept
  {
    return m_LineBufferGPU.IsValid();
  }

  /// Reset all per-frame recording state. Call once at the top of the
  /// frame before any `SetTarget` / `DrawLine` calls.
  void NewFrame();

  /// Begin a new render-target group. All `DrawLine` calls after this
  /// (until the next `SetTarget` or `Submit`) are routed to `target`.
  ///
  /// @param target  Render target to draw subsequent lines into.
  /// @param clear   If non-null, the clear value used when `Submit`
  ///                begins the render pass for this target. If null,
  ///                target contents are loaded.
  void SetTarget(const ::gecko::graphics::RenderTarget& target,
                 const ::gecko::graphics::ClearValue* clear = nullptr);

  /// Record a 2D line in pixel coordinates (top-left origin).
  ///
  /// @param a          Line start in pixels.
  /// @param b          Line end in pixels.
  /// @param color      Linear RGB in `[0, 1]`.
  /// @param thickness  Width in pixels.
  void DrawLine(::gecko::math::float2 a, ::gecko::math::float2 b,
                ::gecko::math::float3 color, ::gecko::f32 thickness);

  /// Upload the recorded lines and issue draw calls. The context owns
  /// the `BeginRendering` / `EndRendering` pair for each target it was
  /// told about, so the caller MUST NOT have an open render pass at
  /// the time of this call and need not close one afterwards.
  ///
  /// @param cmd  Command list to record into. Must be in the
  ///             recording state (between `Begin` and `End`).
  void Submit(::gecko::graphics::ICommandList* cmd);

private:
  struct Line2D
  {
    ::gecko::math::float2 A;      ///< Start, pixels, top-left origin.
    ::gecko::math::float2 B;      ///< End, pixels.
    ::gecko::math::float3 Color;  ///< RGB, linear, [0, 1].
    ::gecko::f32 Thickness;       ///< Width in pixels.
  };

  struct Target
  {
    ::gecko::graphics::RenderTarget RenderTarget {};
    ::gecko::graphics::ClearValue Clear {};
    bool HasClear {false};
    ::gecko::u32 LineBeginIndex {0};
    ::gecko::u32 LineCount {0};
  };

  ::std::vector<Line2D> m_LineBufferCPU {};
  ::gecko::u32 m_CurrentLineIndex {0};

  ::gecko::graphics::Buffer m_LineBufferGPU {};

  ::std::array<Target, MaxTargetsPerFrame> m_Targets {};
  ::gecko::u32 m_CurrentTargetIndex {0};

  // Per-frame "warned once" latches so overflow noise is bounded to a
  // single message per kind per frame.
  bool m_LineOverflowWarned {false};
  bool m_TargetOverflowWarned {false};
};

}  // namespace gecko::debug_renderer
