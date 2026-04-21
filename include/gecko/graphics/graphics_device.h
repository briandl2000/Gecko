#pragma once

#include "gecko/core/api.h"
#include "gecko/core/ptr.h"
#include "gecko/graphics/command_list.h"
#include "gecko/graphics/objects.h"
#include "gecko/platform/window.h"

#include <span>

namespace gecko::graphics {

class IDevice;

/// The main graphics context object.
/// Create one instance per application; pass it by reference to code that
/// needs GPU access. Owns the backend device (NullDevice by default; swap in
/// a concrete backend via the constructor in a future revision).
///
/// Mirrors the pattern of gecko::platform::PlatformContext — no singletons,
/// no global state.
class GraphicsDevice
{
public:
  GECKO_API GraphicsDevice();
  GECKO_API ~GraphicsDevice();

  GraphicsDevice(const GraphicsDevice&)            = delete("GraphicsDevice is non-copyable");
  GraphicsDevice& operator=(const GraphicsDevice&) = delete("GraphicsDevice is non-copyable");

  GraphicsDevice(GraphicsDevice&&) noexcept            = default;
  GraphicsDevice& operator=(GraphicsDevice&&) noexcept = default;

  // ── Swapchain management ──────────────────────────────────────

  [[nodiscard("Discarding a Swapchain leaks GPU resources")]]
  GECKO_API Swapchain CreateSwapchain(
      const ::gecko::platform::NativeWindowHandle& native,
      const ::gecko::platform::WindowDesc&         windowDesc,
      const SwapchainDesc&                         desc) noexcept;

  GECKO_API void DestroySwapchain(Swapchain& swapchain) noexcept;

  GECKO_API void ResizeSwapchain(Swapchain& swapchain, u32 width,
                                 u32 height) noexcept;

  [[nodiscard]]
  GECKO_API RenderTarget GetCurrentBackBuffer(
      const Swapchain& swapchain) const noexcept;

  [[nodiscard]]
  GECKO_API u32 GetCurrentBackBufferIndex(
      const Swapchain& swapchain) const noexcept;

  GECKO_API void Present(const Swapchain& swapchain) noexcept;

  // ── Command lists ─────────────────────────────────────────────

  [[nodiscard("Discarding a CommandList without executing it wastes work")]]
  GECKO_API Unique<ICommandList> CreateGraphicsCommandList() noexcept;

  GECKO_API void ExecuteGraphicsCommandList(
      Unique<ICommandList> commandList) noexcept;

  [[nodiscard("Discarding a CommandList without executing it wastes work")]]
  GECKO_API Unique<ICommandList> CreateComputeCommandList() noexcept;

  GECKO_API void ExecuteComputeCommandList(
      Unique<ICommandList> commandList) noexcept;

  // ── Resource creation ─────────────────────────────────────────

  [[nodiscard("Discarding a created RenderTarget leaks GPU resources")]]
  GECKO_API RenderTarget CreateRenderTarget(
      const RenderTargetDesc& desc) noexcept;

  [[nodiscard("Discarding a created Buffer leaks GPU resources")]]
  GECKO_API Buffer CreateVertexBuffer(const VertexBufferDesc& desc) noexcept;

  [[nodiscard("Discarding a created Buffer leaks GPU resources")]]
  GECKO_API Buffer CreateIndexBuffer(const IndexBufferDesc& desc) noexcept;

  [[nodiscard("Discarding a created Buffer leaks GPU resources")]]
  GECKO_API Buffer CreateConstantBuffer(
      const ConstantBufferDesc& desc) noexcept;

  [[nodiscard("Discarding a created Buffer leaks GPU resources")]]
  GECKO_API Buffer CreateStructuredBuffer(
      const StructuredBufferDesc& desc) noexcept;

  [[nodiscard("Discarding a created Texture leaks GPU resources")]]
  GECKO_API Texture CreateTexture(const TextureDesc& desc) noexcept;

  [[nodiscard("Discarding a created GraphicsPipeline leaks GPU resources")]]
  GECKO_API GraphicsPipeline CreateGraphicsPipeline(
      const GraphicsPipelineDesc& desc) noexcept;

  [[nodiscard("Discarding a created ComputePipeline leaks GPU resources")]]
  GECKO_API ComputePipeline CreateComputePipeline(
      const ComputePipelineDesc& desc) noexcept;

  // ── Data upload ───────────────────────────────────────────────

  GECKO_API void UploadTextureData(Texture& texture,
                                   ::std::span<const ::gecko::byte> data,
                                   u32 mip = 0, u32 slice = 0) noexcept;

  GECKO_API void UploadBufferData(Buffer& buffer,
                                  ::std::span<const ::gecko::byte> data,
                                  u32 offset = 0) noexcept;

private:
  Unique<IDevice> m_Device;
};

}  // namespace gecko::graphics
