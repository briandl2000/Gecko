#pragma once

/// @file
/// `DebugRendererContext` -- per-view recorder of 2D debug lines.
///
/// Lifecycle (per frame):
/// @code
///   cmd->Begin();
///   ctx.NewFrame();                    // CPU: rotate ring, reset cursors
///   cmd->BeginRendering(target, ...);  // user owns the render pass
///   ctx.SetFrame(target);              // bind target to the context
///   ctx.DrawLine(a, b, ...);           // append lines for this pass
///   ctx.Submit(cmd);                   // record draws into the open pass
///   cmd->EndRendering();
///   ctx.EndFrame();                    // upload staging -> GPU; validate
///   cmd->End();
///   device->ExecuteGraphicsCommandList(::std::move(cmd));
/// @endcode
///
/// `Submit` only records draw commands; it does NOT open or close a
/// render pass. The caller is responsible for `BeginRendering` /
/// `EndRendering` and any clears. `SetFrame` tells the context which
/// target the next `Submit` will draw into (used for the viewport
/// push constant today; will carry view/projection in the future).

#include "gecko/math/vector.h"
#include <gecko/core/types.h>
#include <gecko/graphics/command_list.h>
#include <gecko/graphics/graphics_types.h>
#include <gecko/math/math.h>
#include <vector>

namespace gecko::debug_renderer {

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
  explicit DebugRendererContext(::gecko::u32 lineCapacity = DefaultLineCapacity,
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
  /// CPU only. Call exactly once per game frame, before any other
  /// context call. If the previous frame did not call `EndFrame`
  /// after recording lines, a warning is emitted because those lines
  /// never reached the GPU.
  void NewFrame();

  /// Bind the render target the next `Submit` will draw into.
  ///
  /// CPU only. Call inside the open render pass (after
  /// `BeginRendering`) so the binding's lifetime matches the
  /// recorded draws. Used today for the viewport push constant; will
  /// later also carry view/projection for 3D lines.
  ///
  /// `SetFrame` may be called multiple times per frame to fan
  /// batches across multiple targets; each `SetFrame` rebinds and
  /// the next `Submit` consumes the lines accumulated since the
  /// previous `Submit` (or `NewFrame`).
  ///
  /// Format constraint: `target.Desc.Format` must currently be
  /// `R8G8B8A8_UNORM` -- the shared pipeline is created for that
  /// format.
  void SetFrame(::gecko::graphics::RenderTarget target);

  /// Append one 2D line. CPU only; does not touch the command list.
  ///
  /// On overflow the line is dropped and a single warning is emitted
  /// per frame (subsequent overflows in the same frame are silent).
  ///
  /// @param a          Start point in pixels (top-left origin).
  /// @param b          End point in pixels.
  /// @param color      Linear RGB in `[0, 1]`.
  /// @param thickness  Width in pixels.
  void DrawLine(::gecko::math::float2 a, ::gecko::math::float2 b, ::gecko::math::float3 color, ::gecko::f32 thickness);

  void DrawText(const char* text, ::gecko::math::float2 pos,  math::Float3 color, f32 scale);

  /// Record a draw command for the lines added since the last
  /// `Submit` (or since `NewFrame` for the first call this frame).
  ///
  /// The caller MUST already have an open render pass on `cmd`
  /// matching the target last passed to `SetFrame`. `Submit` only
  /// records pipeline / buffer / draw state; it does not open or
  /// close render passes, and does not clear the target.
  ///
  /// @param cmd  Command list in the recording state.
  void Submit(::gecko::graphics::ICommandList* cmd);

  /// Upload the current frame slot's staging buffer to the GPU and
  /// validate frame state.
  ///
  /// Call once per frame, after all `DrawLine` / `Submit` calls and
  /// before executing the command list. The upload is performed on
  /// the device directly (not recorded into `cmd`); we rely on the
  /// device's `UploadBufferData` to be ordered before any subsequent
  /// `ExecuteGraphicsCommandList`. TODO(#debug_renderer): confirm
  /// upload-vs-execute ordering on Vulkan and add an explicit
  /// barrier or copy command if needed.
  ///
  /// Warns if lines were appended but never `Submit`-ted in this
  /// frame (those lines were uploaded but never drawn).
  void EndFrame();

private:
  struct Line2D
  {
    ::gecko::math::float2 A;      ///< Start, pixels, top-left origin.
    ::gecko::math::float2 B;      ///< End, pixels.
    ::gecko::math::float3 Color;  ///< RGB, linear, [0, 1].
    ::gecko::f32 Thickness;       ///< Width in pixels.
  };

  struct Char2D
  {
    ::gecko::math::float2 Position;
    ::gecko::f32 size;
    ::gecko::f32 _Pad;
    ::gecko::math::float3 Color;
    ::gecko::f32 _Pad2;
  };

  struct FrameSlot
  {
    ::std::vector<Line2D> CPU {};
    ::std::vector<Char2D> CPU_chars {};
    ::gecko::graphics::Buffer GPU {};
    ::gecko::graphics::Buffer GPU_chars {};
  };

  ::std::vector<FrameSlot> m_FrameSlots {};
  ::gecko::graphics::RenderTarget m_CurrentTarget {};
  ::gecko::u32 m_LineCapacity {0};
  ::gecko::u32 m_CurrentSlot {0};
  ::gecko::u32 m_LineCursor {0};  ///< Next free slot in the CPU buffer.
  ::gecko::u32 m_BatchStart {0};  ///< First line of the current batch.

  ::gecko::u32 m_CharCapacity {0};
  ::gecko::u32 m_CharCursor {0};
  ::gecko::u32 m_CharBatchStart {0};

  bool m_Valid {false};
  bool m_FrameStarted {false};        ///< NewFrame called, EndFrame pending.
  bool m_FrameUploaded {true};        ///< Last frame's lines reached the GPU.
  bool m_FrameBound {false};          ///< SetFrame called this frame.
  bool m_LineOverflowWarned {false};  ///< Per-frame latch.
  bool m_CharOverflowWarned {false};  ///< Per-frame latch.
};

}  // namespace gecko::debug_renderer
