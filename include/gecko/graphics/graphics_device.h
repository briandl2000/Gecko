#pragma once

/// @file
/// Abstract `GraphicsDevice` and the factory that creates concrete
/// backends. Resource-creation methods return handle types from
/// `graphics_types.h`; the device must outlive every handle it produces.

#include "gecko/core/api.h"
#include "gecko/core/ptr.h"
#include "gecko/core/types.h"
#include "gecko/graphics/command_list.h"
#include "gecko/graphics/gpu_profiler.h"
#include "gecko/graphics/graphics_types.h"
#include "gecko/platform/window.h"

#include <span>

namespace gecko::graphics {

// Backend selection -------------------------------------------------

/// Concrete backend behind a `GraphicsDevice`.
enum class GraphicsBackend : u8
{
  Null,    ///< Zero-dep no-op backend (default).
  Vulkan,  ///< Vulkan 1.3 backend.
};

/// Parameters for `CreateGraphicsDevice`.
struct GraphicsDeviceDesc
{
  GraphicsBackend Backend {GraphicsBackend::Null};  ///< Which backend to instantiate.
  bool Debug {false};                               ///< Enable validation layers / GPU-side checks.
  const char* AppName {"Gecko"};                    ///< Identifier reported to the driver.
};

// -- Frame context ---------------------------------------------------------

/// Returned by `BeginFrame`. Carries the acquired back buffer plus
/// the sync slot used for that acquisition. Cheap to copy; must be
/// passed to `Present` (directly or via a span) to complete the frame.
struct FrameContext
{
  Swapchain* SC {nullptr};     ///< Source swapchain pointer.
  u32 FrameIndex {0};          ///< Sync slot in `[0, MaxFramesInFlight)`.
  u32 ImageIndex {0};          ///< Acquired swapchain image index.
  RenderTarget BackBuffer {};  ///< Render target for the acquired image.
  bool Valid {false};          ///< `true` once acquisition succeeded.
};

// -- GraphicsDevice --------------------------------------------------------

/// Abstract graphics device. Concrete backends (`NullDevice`, `VulkanDevice`,
/// ...) inherit from this class. Users receive a `Unique<GraphicsDevice>`
/// from `CreateGraphicsDevice()` and pass `GraphicsDevice&` to code that
/// needs GPU access.
///
/// Ownership rule: GPU objects (Buffer, Texture, Swapchain, Pipeline,
/// RenderTarget) hold deleters that reference the owning device. The device
/// **must outlive** all GPU objects it created.
class GraphicsDevice
{
public:
  virtual ~GraphicsDevice() = default;

  GraphicsDevice(const GraphicsDevice&) = delete;
  GraphicsDevice& operator=(const GraphicsDevice&) = delete;

  GraphicsDevice(GraphicsDevice&&) noexcept = default;
  GraphicsDevice& operator=(GraphicsDevice&&) noexcept = default;

  // Swapchain management ---------------------------------------

  /// Create a swapchain bound to `native` window with `desc`.
  [[nodiscard("Swapchain must be stored to present frames")]]
  GECKO_API virtual Swapchain CreateSwapchain(const ::gecko::platform::NativeWindowHandle& native,
                                              const SwapchainDesc& desc) noexcept = 0;

  /// Destroy a swapchain. The handle is left in an invalid state.
  GECKO_API virtual void DestroySwapchain(Swapchain& swapchain) noexcept = 0;

  /// Re-create the underlying surface to match its window's current size.
  GECKO_API virtual void ResizeSwapchain(Swapchain& swapchain) noexcept = 0;

  /// Acquire the next swapchain image and wait on its frame fence.
  /// Returns a `FrameContext` whose `BackBuffer` can be rendered into and
  /// which must later be passed to `Present`.
  [[nodiscard]]
  GECKO_API virtual FrameContext BeginFrame(Swapchain& swapchain) noexcept = 0;

  /// Present one or more swapchains in a single driver call.
  GECKO_API virtual void Present(::std::span<const FrameContext> frames) noexcept = 0;

  /// Convenience overload for the common single-swapchain case.
  void Present(const FrameContext& frame) noexcept
  {
    Present(::std::span<const FrameContext> {&frame, 1});
  }

  // Command lists ----------------------------------------------

  /// Allocate a graphics command list. Caller fills it and submits via
  /// `ExecuteGraphicsCommandList`.
  [[nodiscard("Discarding a CommandList without executing it wastes work")]]
  GECKO_API virtual Unique<ICommandList> CreateGraphicsCommandList() noexcept = 0;

  /// Allocate a compute command list.
  [[nodiscard("Discarding a CommandList without executing it wastes work")]]
  GECKO_API virtual Unique<ICommandList> CreateComputeCommandList() noexcept = 0;

  /// Submit a graphics command list and reclaim its allocator.
  GECKO_API virtual void ExecuteGraphicsCommandList(Unique<ICommandList> commandList) noexcept = 0;

  /// Submit a compute command list and reclaim its allocator.
  GECKO_API virtual void ExecuteComputeCommandList(Unique<ICommandList> commandList) noexcept = 0;

  // Resource creation ------------------------------------------

  [[nodiscard("Discarding a RenderTarget immediately releases the GPU resource")]]
  GECKO_API virtual RenderTarget CreateRenderTarget(const RenderTargetDesc& desc) noexcept = 0;

  [[nodiscard("Discarding a created Buffer immediately releases the GPU resource")]]
  GECKO_API virtual Buffer CreateVertexBuffer(const VertexBufferDesc& desc) noexcept = 0;

  [[nodiscard("Discarding a created Buffer immediately releases the GPU resource")]]
  GECKO_API virtual Buffer CreateIndexBuffer(const IndexBufferDesc& desc) noexcept = 0;

  [[nodiscard("Discarding a created Buffer immediately releases the GPU resource")]]
  GECKO_API virtual Buffer CreateConstantBuffer(const ConstantBufferDesc& desc) noexcept = 0;

  [[nodiscard("Discarding a created Buffer immediately releases the GPU resource")]]
  GECKO_API virtual Buffer CreateStructuredBuffer(const StructuredBufferDesc& desc) noexcept = 0;

  [[nodiscard("Discarding a created Texture immediately releases the GPU resource")]]
  GECKO_API virtual Texture CreateTexture(const TextureDesc& desc) noexcept = 0;

  [[nodiscard("Discarding a created Sampler immediately releases the GPU resource")]]
  GECKO_API virtual Sampler CreateSampler(const SamplerDesc& desc) noexcept = 0;

  [[nodiscard("Discarding a created GraphicsPipeline immediately releases the "
              "GPU resource")]]
  GECKO_API virtual GraphicsPipeline CreateGraphicsPipeline(const GraphicsPipelineDesc& desc) noexcept = 0;

  [[nodiscard("Discarding a created ComputePipeline immediately releases the "
              "GPU resource")]]
  GECKO_API virtual ComputePipeline CreateComputePipeline(const ComputePipelineDesc& desc) noexcept = 0;

  [[nodiscard("Discarding a created QueryPool immediately releases the GPU resource")]]
  GECKO_API virtual QueryPool CreateTimestampQueryPool(const QueryPoolDesc& desc) noexcept = 0;

  /// Read `count` timestamps starting at `firstQuery` from the pool into
  /// `out` as nanoseconds. Returns the number of timestamps written (0 on
  /// failure or if results are not yet available on the implementation).
  GECKO_API virtual u32 ReadTimestamps(const QueryPool& pool, u32 firstQuery, ::std::span<u64> out) noexcept = 0;

  // GPU profiler -----------------------------------------------

  /// Create a GPU sampler. Returns `nullptr` on backends that have no
  /// timestamp queries (NullDevice).
  [[nodiscard("Discarding the returned IGpuSampler immediately releases the "
              "GPU resources backing it")]]
  GECKO_API virtual ::gecko::Unique<IGpuSampler> CreateGpuSampler(const GpuSamplerDesc& desc) noexcept = 0;

  // Data upload ------------------------------------------------

  /// Upload `data` into a mip+slice of `texture`. Stages through a
  /// CPU-visible buffer when the texture is `Dedicated`.
  GECKO_API virtual void UploadTextureData(Texture& texture, ::std::span<const ::gecko::byte> data, u32 mip = 0,
                                           u32 slice = 0) noexcept = 0;

  /// Upload `data` into `buffer` starting at `offset` bytes.
  GECKO_API virtual void UploadBufferData(Buffer& buffer, ::std::span<const ::gecko::byte> data,
                                          u32 offset = 0) noexcept = 0;

protected:
  GraphicsDevice() = default;
};

// Factory -----------------------------------------------------------

/// Create a `NullDevice` (zero-dep, no-op). Useful for headless tests.
[[nodiscard]]
GECKO_API Unique<GraphicsDevice> CreateGraphicsDevice() noexcept;

/// Create a graphics device with the specified backend and options.
[[nodiscard]]
GECKO_API Unique<GraphicsDevice> CreateGraphicsDevice(const GraphicsDeviceDesc& desc) noexcept;

}  // namespace gecko::graphics
