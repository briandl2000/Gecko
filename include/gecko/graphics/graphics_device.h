#pragma once

#include "gecko/core/api.h"
#include "gecko/core/ptr.h"
#include "gecko/graphics/command_list.h"
#include "gecko/graphics/objects.h"
#include "gecko/platform/window.h"

#include <span>

namespace gecko::graphics {

/// Abstract graphics device. Concrete backends (NullDevice, VulkanDevice, ...)
/// inherit from this class. Users receive a Unique<GraphicsDevice> from
/// CreateGraphicsDevice() and pass GraphicsDevice& to code that needs GPU access.
///
/// No singleton — no global state. Create one, pass it around.
class GraphicsDevice
{
public:
  virtual ~GraphicsDevice() = default;

  GraphicsDevice(const GraphicsDevice&)            = delete("GraphicsDevice is non-copyable");
  GraphicsDevice& operator=(const GraphicsDevice&) = delete("GraphicsDevice is non-copyable");

  GraphicsDevice(GraphicsDevice&&) noexcept            = default;
  GraphicsDevice& operator=(GraphicsDevice&&) noexcept = default;

  // ── Swapchain management ──────────────────────────────────────

  [[nodiscard("Discarding a Swapchain leaks GPU resources")]]
  GECKO_API virtual Swapchain CreateSwapchain(
      const ::gecko::platform::NativeWindowHandle& native,
      const ::gecko::platform::WindowDesc&         windowDesc,
      const SwapchainDesc&                         desc) noexcept = 0;

  GECKO_API virtual void DestroySwapchain(Swapchain& swapchain) noexcept = 0;

  GECKO_API virtual void ResizeSwapchain(Swapchain& swapchain, u32 width,
                                         u32 height) noexcept = 0;

  [[nodiscard]]
  GECKO_API virtual RenderTarget GetCurrentBackBuffer(
      const Swapchain& swapchain) const noexcept = 0;

  [[nodiscard]]
  GECKO_API virtual u32 GetCurrentBackBufferIndex(
      const Swapchain& swapchain) const noexcept = 0;

  GECKO_API virtual void Present(const Swapchain& swapchain) noexcept = 0;

  // ── Command lists ─────────────────────────────────────────────

  [[nodiscard("Discarding a CommandList without executing it wastes work")]]
  GECKO_API virtual Unique<ICommandList>
  CreateGraphicsCommandList() noexcept = 0;

  GECKO_API virtual void ExecuteGraphicsCommandList(
      Unique<ICommandList> commandList) noexcept = 0;

  [[nodiscard("Discarding a CommandList without executing it wastes work")]]
  GECKO_API virtual Unique<ICommandList>
  CreateComputeCommandList() noexcept = 0;

  GECKO_API virtual void ExecuteComputeCommandList(
      Unique<ICommandList> commandList) noexcept = 0;

  // ── Resource creation ─────────────────────────────────────────

  [[nodiscard("Discarding a created RenderTarget leaks GPU resources")]]
  GECKO_API virtual RenderTarget CreateRenderTarget(
      const RenderTargetDesc& desc) noexcept = 0;

  [[nodiscard("Discarding a created Buffer leaks GPU resources")]]
  GECKO_API virtual Buffer CreateVertexBuffer(
      const VertexBufferDesc& desc) noexcept = 0;

  [[nodiscard("Discarding a created Buffer leaks GPU resources")]]
  GECKO_API virtual Buffer CreateIndexBuffer(
      const IndexBufferDesc& desc) noexcept = 0;

  [[nodiscard("Discarding a created Buffer leaks GPU resources")]]
  GECKO_API virtual Buffer CreateConstantBuffer(
      const ConstantBufferDesc& desc) noexcept = 0;

  [[nodiscard("Discarding a created Buffer leaks GPU resources")]]
  GECKO_API virtual Buffer CreateStructuredBuffer(
      const StructuredBufferDesc& desc) noexcept = 0;

  [[nodiscard("Discarding a created Texture leaks GPU resources")]]
  GECKO_API virtual Texture CreateTexture(
      const TextureDesc& desc) noexcept = 0;

  [[nodiscard("Discarding a created GraphicsPipeline leaks GPU resources")]]
  GECKO_API virtual GraphicsPipeline CreateGraphicsPipeline(
      const GraphicsPipelineDesc& desc) noexcept = 0;

  [[nodiscard("Discarding a created ComputePipeline leaks GPU resources")]]
  GECKO_API virtual ComputePipeline CreateComputePipeline(
      const ComputePipelineDesc& desc) noexcept = 0;

  // ── Data upload ───────────────────────────────────────────────

  GECKO_API virtual void UploadTextureData(
      Texture& texture, ::std::span<const ::gecko::byte> data, u32 mip = 0,
      u32 slice = 0) noexcept = 0;

  GECKO_API virtual void UploadBufferData(
      Buffer& buffer, ::std::span<const ::gecko::byte> data,
      u32 offset = 0) noexcept = 0;

protected:
  GraphicsDevice() = default;
};

// ── Factory ───────────────────────────────────────────────────────────────

/// Create a graphics device. Returns a NullDevice until a concrete backend
/// is wired in (e.g. Vulkan). Future overload will accept a config struct.
[[nodiscard]]
GECKO_API Unique<GraphicsDevice> CreateGraphicsDevice() noexcept;

}  // namespace gecko::graphics
