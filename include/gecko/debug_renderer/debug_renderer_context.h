#pragma once

/// @file
/// `DebugRendererContext` -- per-view recorder of 2D debug lines.
///
/// Lifecycle (per frame):
/// @code
///   ctx.NewFrame();                    // CPU: rotate ring, reset cursors
///   ctx.DrawLine(a, b, ...);           // CPU: append to staging buffer
///   ...
///   cmd->BeginRendering(target, ...);  // user owns the render pass
///   ctx.Submit(cmd, {.Target = target});
///   cmd->EndRendering();
///   ...
///   ctx.EndFrame();                    // upload staging -> GPU; once per
///   frame device->ExecuteGraphicsCommandList(::std::move(cmd));
/// @endcode
///
/// `DrawLine` is purely CPU and may be called any time -- before, between,
/// or after `BeginRendering` / `Submit` calls. `Submit` only records draw
/// commands; it does NOT open or close a render pass. The caller is
/// responsible for `BeginRendering` / `EndRendering` and any clears.

#include <gecko/core/types.h>
#include <gecko/graphics/command_list.h>
#include <gecko/graphics/graphics_types.h>
#include <gecko/math/math.h>
#include <vector>

namespace gecko::debug_renderer {

/// Per-`Submit` parameters. Today only carries the color attachment;
/// reserved for `Depth` and `ViewProj` once 3D lines land so the call
/// site doesn't need to change.
struct DebugRendererSubmitInfo
{
  /// Color attachment the lines will be rasterised into. Must already
  /// be bound by an open `BeginRendering` on the command list passed
  /// to `Submit`. Width/Height are read from `Target.Desc` for the
  /// pixel-to-NDC mapping in the line shader.
  ///
  /// Format constraint: `Target.Desc.Format` must currently be
  /// `R8G8B8A8_UNORM` -- the shared pipeline is created for that
  /// format. Mismatches will fail at validation time on Vulkan.
  ::gecko::graphics::RenderTarget Target {};

  // Reserved for 3D / future:
  // const ::gecko::graphics::RenderTarget* Depth = nullptr;
  // ::gecko::math::float4x4 ViewProj = ::gecko::math::Identity4x4();
};

/// Records 2D debug lines and submits them to a graphics command list.
///
/// Owns a ring of CPU staging buffers and matching GPU structured
/// buffers, sized to `framesInFlight` so a frame's GPU buffer isn't
/// being read by an in-flight prior frame while the CPU writes it.
///
/// Lifetime: the context owns GPU buffers; it MUST be destroyed
/// before the `GraphicsDevice` (i.e. before `GraphicsModule` shuts
/// down).
class DebugRendererContext
{
public:
  /// Default per-frame line capacity (~4 MB at `sizeof(Line2D) == 32`).
  static constexpr ::gecko::u32 DefaultLineCapacity = 128u * 1024u;

  /// Default ring size. Should match the swapchain's frames-in-flight.
  static constexpr ::gecko::u32 DefaultFramesInFlight = 2;

  /// Hard cap on ring size as a sanity guard.
  static constexpr ::gecko::u32 MaxFramesInFlight = 4;

  /// Construct a context.
  ///
  /// @param lineCapacity    Maximum lines recordable per frame.
  ///                        Must be > 0.
  /// @param framesInFlight  Ring size; should be at least the
  ///                        swapchain's frames-in-flight. Must be in
  ///                        `[1, MaxFramesInFlight]`.
  explicit DebugRendererContext(
      ::gecko::u32 lineCapacity = DefaultLineCapacity,
      ::gecko::u32 framesInFlight = DefaultFramesInFlight);
  ~DebugRendererContext() = default;

  DebugRendererContext(const DebugRendererContext&) = delete;
  DebugRendererContext& operator=(const DebugRendererContext&) = delete;

  /// True if every ring slot's GPU buffer was created successfully.
  [[nodiscard]] bool IsValid() const noexcept
  {
    return m_Valid;
  }

  /// Rotate to the next ring slot and reset per-frame cursors.
  ///
  /// CPU only. Call exactly once per game frame, before any
  /// `DrawLine` / `Submit` calls on this context. If the previous
  /// frame did not call `EndFrame` after recording lines, a warning
  /// is emitted because those lines never reached the GPU.
  void NewFrame();

  /// Append one 2D line. CPU only; does not touch the command list
  /// and may be called any time during a frame.
  ///
  /// On overflow the line is dropped and a single warning is emitted
  /// per frame (subsequent overflows in the same frame are silent).
  ///
  /// @param a          Start point in pixels (top-left origin).
  /// @param b          End point in pixels.
  /// @param color      Linear RGB in `[0, 1]`.
  /// @param thickness  Width in pixels.
  void DrawLine(::gecko::math::float2 a, ::gecko::math::float2 b,
                ::gecko::math::float3 color, ::gecko::f32 thickness);

  /// Record a draw command for the lines added since the last
  /// `Submit` (or since `NewFrame` for the first call this frame).
  ///
  /// The caller MUST already have an open render pass on `cmd`
  /// (i.e. between `BeginRendering` and `EndRendering`) targeting
  /// `info.Target`. `Submit` only records pipeline / buffer / draw
  /// state; it does not open or close render passes, and does not
  /// clear the target.
  ///
  /// Subsequent `DrawLine` calls accumulate into the next batch;
  /// later `Submit` calls draw only that next batch.
  ///
  /// @param cmd   Command list in the recording state.
  /// @param info  Submit parameters (color target today, more later).
  void Submit(::gecko::graphics::ICommandList* cmd,
              const DebugRendererSubmitInfo& info);

  /// Upload the current frame slot's staging buffer to the GPU.
  ///
  /// Call once per frame, after all `DrawLine` / `Submit` calls and
  /// before executing the command list. The upload is performed on
  /// the device directly (not recorded into `cmd`); we rely on the
  /// device's `UploadBufferData` to be ordered before any subsequent
  /// `ExecuteGraphicsCommandList`. TODO(#debug_renderer): confirm
  /// upload-vs-execute ordering on Vulkan and add an explicit
  /// barrier or copy command if needed.
  void EndFrame();

private:
  struct Line2D
  {
    ::gecko::math::float2 A;      ///< Start, pixels, top-left origin.
    ::gecko::math::float2 B;      ///< End, pixels.
    ::gecko::math::float3 Color;  ///< RGB, linear, [0, 1].
    ::gecko::f32 Thickness;       ///< Width in pixels.
  };

  struct FrameSlot
  {
    ::std::vector<Line2D> CPU {};
    ::gecko::graphics::Buffer GPU {};
  };

  ::std::vector<FrameSlot> m_FrameSlots {};
  ::gecko::u32 m_LineCapacity {0};
  ::gecko::u32 m_CurrentSlot {0};
  ::gecko::u32 m_LineCursor {0};  ///< Next free slot in the CPU buffer.
  ::gecko::u32 m_BatchStart {0};  ///< First line of the current batch.

  bool m_Valid {false};
  bool m_FrameStarted {false};        ///< NewFrame called, EndFrame pending.
  bool m_FrameUploaded {true};        ///< Last frame's lines reached the GPU.
  bool m_LineOverflowWarned {false};  ///< Per-frame latch.
};

}  // namespace gecko::debug_renderer
