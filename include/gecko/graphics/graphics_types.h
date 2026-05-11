#pragma once

#include "gecko/core/ptr.h"
#include "gecko/core/span.h"
#include "gecko/core/types.h"

#include <span>

namespace gecko::graphics {

// -- Named constants -------------------------------------------------------

inline constexpr u32 MaxSwapchainImages = 8;
inline constexpr u32 MaxFramesInFlight = 2;
inline constexpr u32 MaxSwapchainsPerSubmit = 4;
inline constexpr u32 MaxPushConstantBytes = 128;

// -- Enums -----------------------------------------------------------------

enum class ShaderType : u8
{
  All,
  Vertex,
  Pixel,
  Compute,
};

enum class DataFormat : u16
{
  None,
  // Colour
  R8G8B8A8_SRGB,
  R8G8B8A8_UNORM,
  B8G8R8A8_UNORM,
  B8G8R8A8_SRGB,
  R32G32_FLOAT,
  R32G32B32_FLOAT,
  R32G32B32A32_FLOAT,
  R16G16B16A16_FLOAT,
  R32_FLOAT,
  R8_UINT,
  R16_UINT,
  R32_UINT,
  R8_INT,
  R16_INT,
  R32_INT,
  // Depth / depth-stencil
  D32_FLOAT,
  D24_UNORM_S8_UINT,
  D16_UNORM,
};

/// Source format of a `ShaderCode` blob.
enum class ShaderFormat : u8
{
  None,
  SPIRV,        ///< Vulkan native; portable for runtime translation.
  DXIL,         ///< DX12 native.
  GLSL_Source,  ///< Runtime-compiled (future).
  HLSL_Source,  ///< Runtime-compiled (future).
};

/// Discriminator for a `ClearValue`.
enum class ClearValueType : u8
{
  RenderTarget,  ///< `Color` is meaningful.
  DepthStencil,  ///< `Depth` and `Stencil` are meaningful.
};

/// Logical texture dimension / array-ness.
enum class TextureType : u8
{
  None,
  Tex1D,
  Tex2D,
  Tex3D,
  TexCube,
  Tex1DArray,
  Tex2DArray,
};

/// Triangle culling mode for a graphics pipeline.
enum class CullMode : u8
{
  None,
  Back,
  Front,
};

/// Front-face winding order for triangle culling.
enum class WindingOrder : u8
{
  ClockWise,
  CounterClockWise,
};

/// Primitive topology fed to the rasterizer.
enum class PrimitiveType : u8
{
  Lines,
  Triangles,
};

/// Texture filter mode for a `Sampler`.
enum class SamplerFilter : u8
{
  Linear,
  Point,
};

/// Address-mode applied to texture coordinates outside `[0, 1]`.
enum class SamplerWrapMode : u8
{
  Wrap,
  Clamp,
};

/// Kind of resource bound at a pipeline-resource slot.
enum class ResourceType : u8
{
  None,
  Texture,             ///< Read-only sampled texture.
  RWTexture,           ///< Read-write storage texture (UAV).
  ConstantBuffer,      ///< Small uniform block.
  StructuredBuffer,    ///< Read-only structured buffer.
  RWStructuredBuffer,  ///< Read-write structured buffer (UAV).
  Sampler,             ///< Standalone sampler object.
  LocalData,           ///< Push-constant / inline-data block.
};

/// `Buffer` usage class. Determines which `*Desc` is meaningful.
enum class BufferType : u8
{
  None,
  Vertex,
  Index,
  Constant,
  Structured,
};

/// Memory residency hint for buffer / texture allocations.
enum class MemoryType : u8
{
  None,
  Shared,     ///< CPU-visible (upload / readback).
  Dedicated,  ///< GPU-only (fast path).
};

/// Comparison function for depth, stencil and shader sampler ops.
enum class CompareFunc : u8
{
  Never,
  Less,
  Equal,
  LessEqual,
  Greater,
  NotEqual,
  GreaterEqual,
  Always,
};

/// Stencil-buffer operation taken on test result.
enum class StencilOp : u8
{
  Keep,
  Zero,
  Replace,
  IncrementClamp,
  DecrementClamp,
  Invert,
  IncrementWrap,
  DecrementWrap,
};

/// Source / destination factor for the blending equation.
enum class BlendFactor : u8
{
  Zero,
  One,
  SrcColor,
  InvSrcColor,
  SrcAlpha,
  InvSrcAlpha,
  DstColor,
  InvDstColor,
  DstAlpha,
  InvDstAlpha,
  SrcAlphaSaturate,
};

/// Combine operation applied between source and destination factors.
enum class BlendOp : u8
{
  Add,
  Subtract,
  ReverseSubtract,
  Min,
  Max,
};

/// Per-channel write mask for a render-target blend state. Bitwise.
enum class ColorWriteMask : u8
{
  None = 0,
  Red = 1 << 0,
  Green = 1 << 1,
  Blue = 1 << 2,
  Alpha = 1 << 3,
  All = Red | Green | Blue | Alpha,
};

// Utility functions -------------------------------------------------

/// Returns the size in bytes of one element of `format`, or `0` if
/// the format is not a supported single-element type.
[[nodiscard]] constexpr u32 FormatSizeInBytes(DataFormat format) noexcept
{
  switch (format)
  {
  case DataFormat::R8G8B8A8_SRGB:
  case DataFormat::R8G8B8A8_UNORM:
  case DataFormat::B8G8R8A8_UNORM:
  case DataFormat::B8G8R8A8_SRGB:
    return 4;
  case DataFormat::R32G32_FLOAT:
    return 8;
  case DataFormat::R32G32B32_FLOAT:
    return 12;
  case DataFormat::R32G32B32A32_FLOAT:
    return 16;
  case DataFormat::R16G16B16A16_FLOAT:
    return 8;
  case DataFormat::R32_FLOAT:
    return 4;
  case DataFormat::R8_UINT:
  case DataFormat::R8_INT:
    return 1;
  case DataFormat::R16_UINT:
  case DataFormat::R16_INT:
    return 2;
  case DataFormat::R32_UINT:
  case DataFormat::R32_INT:
    return 4;
  case DataFormat::D32_FLOAT:
    return 4;
  case DataFormat::D24_UNORM_S8_UINT:
    return 4;
  case DataFormat::D16_UNORM:
    return 2;
  default:
    return 0;
  }
}

/// `true` if `format` is a depth or depth-stencil format.
[[nodiscard]] constexpr bool IsDepthFormat(DataFormat format) noexcept
{
  switch (format)
  {
  case DataFormat::D32_FLOAT:
  case DataFormat::D24_UNORM_S8_UINT:
  case DataFormat::D16_UNORM:
    return true;
  default:
    return false;
  }
}

/// `true` if `format` includes a stencil channel.
[[nodiscard]] constexpr bool HasStencilComponent(DataFormat format) noexcept
{
  switch (format)
  {
  case DataFormat::D24_UNORM_S8_UINT:
    return true;
  default:
    return false;
  }
}

/// Compute the full mip-chain count for a 2D texture of `width` x
/// `height`. Returns `0` if either dimension is zero.
[[nodiscard]] constexpr u32 CalculateNumberOfMips(u32 width, u32 height) noexcept
{
  if (width == 0 || height == 0)
    return 0;
  u32 mips = 1;
  while (width > 1 || height > 1)
  {
    width = (width > 1) ? width / 2 : 1;
    height = (height > 1) ? height / 2 : 1;
    ++mips;
  }
  return mips;
}

// Descriptor structs ------------------------------------------------

/// One vertex-stream attribute. `Size` and `Offset` are filled in by
/// `VertexLayout::AddAttribute` from `AttributeFormat`.
struct VertexAttribute
{
  const char* Name {"VertexAttribute"};
  DataFormat AttributeFormat {DataFormat::None};
  u32 Size {0};
  u32 Offset {0};

  VertexAttribute() = default;

  VertexAttribute(DataFormat format, const char* name) noexcept
      : Name(name), AttributeFormat(format), Size(FormatSizeInBytes(format)), Offset(0)
  {}

  [[nodiscard]] bool IsValid() const noexcept
  {
    return AttributeFormat != DataFormat::None && Size > 0;
  }
  explicit operator bool() const noexcept
  {
    return IsValid();
  }

  [[nodiscard]] bool operator==(const VertexAttribute& other) const noexcept
  {
    return AttributeFormat == other.AttributeFormat && Size == other.Size && Offset == other.Offset;
  }
};

/// Ordered set of vertex attributes plus a derived stride. Build by
/// repeatedly calling `AddAttribute()`; each attribute is appended
/// tightly packed.
struct VertexLayout
{
  static constexpr u32 MaxAttributes = 16;

  VertexAttribute Attributes[MaxAttributes] {};
  u32 NumAttributes {0};
  u32 StrideInBytes {0};

  VertexLayout() = default;

  void AddAttribute(DataFormat format, const char* name) noexcept
  {
    if (NumAttributes >= MaxAttributes)
      return;
    const u32 size = FormatSizeInBytes(format);
    if (size == 0)
      return;
    VertexAttribute& attr = Attributes[NumAttributes];
    attr.Name = name;
    attr.AttributeFormat = format;
    attr.Size = size;
    attr.Offset = StrideInBytes;
    StrideInBytes += size;
    ++NumAttributes;
  }

  [[nodiscard]] bool IsValid() const noexcept
  {
    if (NumAttributes == 0 || StrideInBytes == 0)
      return false;
    for (u32 i = 0; i < NumAttributes; ++i)
    {
      if (!Attributes[i].IsValid())
        return false;
    }
    return true;
  }
  explicit operator bool() const noexcept
  {
    return IsValid();
  }
};

/// Clear value for a render-target or depth-stencil attachment. Use
/// the `RenderTarget(...)` and `DepthStencil(...)` factories.
struct ClearValue
{
  ClearValueType Type {ClearValueType::RenderTarget};
  f32 Color[4] {0.0F, 0.0F, 0.0F, 1.0F};
  f32 Depth {1.0F};
  u8 Stencil {0};

  [[nodiscard]] static constexpr ClearValue RenderTarget(f32 r, f32 g, f32 b, f32 a) noexcept
  {
    ClearValue cv;
    cv.Type = ClearValueType::RenderTarget;
    cv.Color[0] = r;
    cv.Color[1] = g;
    cv.Color[2] = b;
    cv.Color[3] = a;
    return cv;
  }

  [[nodiscard]] static constexpr ClearValue DepthStencil(f32 depth, u8 stencil) noexcept
  {
    ClearValue cv;
    cv.Type = ClearValueType::DepthStencil;
    cv.Depth = depth;
    cv.Stencil = stencil;
    return cv;
  }
};

/// Sampler creation parameters.
struct SamplerDesc
{
  SamplerFilter Filter {SamplerFilter::Linear};
  SamplerWrapMode WrapMode {SamplerWrapMode::Wrap};
  const char* DebugName {nullptr};

  [[nodiscard]] bool IsValid() const noexcept
  {
    return true;
  }
};

/// Opaque sampler handle. The device must outlive every `Sampler`.
struct Sampler
{
  SamplerDesc Desc {};
  Shared<void> Data {nullptr};

  [[nodiscard]] bool IsValid() const noexcept
  {
    return Data != nullptr;
  }
  explicit operator bool() const noexcept
  {
    return IsValid();
  }
};

/// One pipeline binding-table slot. Use the `*Binding(...)` factories.
struct PipelineResource
{
  ResourceType Type {ResourceType::None};
  ShaderType ShaderVisibility {ShaderType::All};
  u32 NumResources {1};

  [[nodiscard]] static constexpr PipelineResource TextureBinding(u32 count, ShaderType visibility) noexcept
  {
    return PipelineResource {
        .Type = ResourceType::Texture,
        .ShaderVisibility = visibility,
        .NumResources = count,
    };
  }

  [[nodiscard]] static constexpr PipelineResource RWTextureBinding(u32 count, ShaderType visibility) noexcept
  {
    return PipelineResource {
        .Type = ResourceType::RWTexture,
        .ShaderVisibility = visibility,
        .NumResources = count,
    };
  }

  [[nodiscard]] static constexpr PipelineResource ConstantBufferBinding(u32 count, ShaderType visibility) noexcept
  {
    return PipelineResource {
        .Type = ResourceType::ConstantBuffer,
        .ShaderVisibility = visibility,
        .NumResources = count,
    };
  }

  [[nodiscard]] static constexpr PipelineResource StructuredBufferBinding(u32 count, ShaderType visibility) noexcept
  {
    return PipelineResource {
        .Type = ResourceType::StructuredBuffer,
        .ShaderVisibility = visibility,
        .NumResources = count,
    };
  }

  [[nodiscard]] static constexpr PipelineResource RWStructuredBufferBinding(u32 count, ShaderType visibility) noexcept
  {
    return PipelineResource {
        .Type = ResourceType::RWStructuredBuffer,
        .ShaderVisibility = visibility,
        .NumResources = count,
    };
  }

  [[nodiscard]] static constexpr PipelineResource SamplerBinding(u32 count, ShaderType visibility) noexcept
  {
    return PipelineResource {
        .Type = ResourceType::Sampler,
        .ShaderVisibility = visibility,
        .NumResources = count,
    };
  }

  [[nodiscard]] bool IsValid() const noexcept
  {
    return Type != ResourceType::None && NumResources > 0;
  }
  explicit operator bool() const noexcept
  {
    return IsValid();
  }
};

/// Per-face stencil operations and compare function.
struct StencilOpDesc
{
  StencilOp Fail {StencilOp::Keep};
  StencilOp DepthFail {StencilOp::Keep};
  StencilOp Pass {StencilOp::Keep};
  CompareFunc Compare {CompareFunc::Always};
};

/// Combined depth- and stencil-test state for a graphics pipeline.
struct DepthStencilState
{
  bool DepthTestEnable {false};
  bool DepthWriteEnable {false};
  CompareFunc DepthCompare {CompareFunc::Less};
  bool StencilEnable {false};
  u8 StencilReadMask {0xFF};
  u8 StencilWriteMask {0xFF};
  StencilOpDesc StencilFront {};
  StencilOpDesc StencilBack {};
};

/// Per-render-target blend state. Disabled by default.
struct RenderTargetBlendState
{
  bool BlendEnable {false};
  BlendFactor SrcColor {BlendFactor::One};
  BlendFactor DstColor {BlendFactor::Zero};
  BlendOp ColorOp {BlendOp::Add};
  BlendFactor SrcAlpha {BlendFactor::One};
  BlendFactor DstAlpha {BlendFactor::Zero};
  BlendOp AlphaOp {BlendOp::Add};
  u8 WriteMask {static_cast<u8>(ColorWriteMask::All)};
};

// Buffer descriptors & object --------------------------------------

/// Description of a `Buffer` of type `Vertex`.
struct VertexBufferDesc
{
  u32 NumVertices {0};
  u32 VertexSize {0};
  MemoryType Memory {MemoryType::None};
  const char* DebugName {nullptr};

  [[nodiscard]] bool IsValid() const noexcept
  {
    return NumVertices > 0 && VertexSize > 0 && Memory != MemoryType::None;
  }
  explicit operator bool() const noexcept
  {
    return IsValid();
  }
};

/// Description of a `Buffer` of type `Index` (32-bit indices).
struct IndexBufferDesc
{
  u32 NumIndices {0};
  MemoryType Memory {MemoryType::None};
  const char* DebugName {nullptr};

  [[nodiscard]] bool IsValid() const noexcept
  {
    return NumIndices > 0 && Memory != MemoryType::None;
  }
  explicit operator bool() const noexcept
  {
    return IsValid();
  }
};

/// Description of a `Buffer` of type `Constant` (uniform block).
struct ConstantBufferDesc
{
  u32 SizeInBytes {0};
  MemoryType Memory {MemoryType::None};
  const char* DebugName {nullptr};

  [[nodiscard]] bool IsValid() const noexcept
  {
    return SizeInBytes > 0 && Memory != MemoryType::None;
  }
  explicit operator bool() const noexcept
  {
    return IsValid();
  }
};

/// Description of a `Buffer` of type `Structured`.
struct StructuredBufferDesc
{
  u32 NumElements {0};
  u32 ElementSize {0};
  MemoryType Memory {MemoryType::None};
  bool AllowUnorderedAccess {false};
  const char* DebugName {nullptr};

  [[nodiscard]] bool IsValid() const noexcept
  {
    return NumElements > 0 && ElementSize > 0 && Memory != MemoryType::None;
  }
  explicit operator bool() const noexcept
  {
    return IsValid();
  }
};

/// Opaque buffer handle. Holds the matching `*Desc` for the active
/// `Type` plus a shared device-pointer payload. The owning device
/// must outlive every `Buffer`.
/// Opaque buffer handle. Holds the matching `*Desc` for the active
/// `Type` plus a shared device-pointer payload. The owning device
/// must outlive every `Buffer`.
struct Buffer
{
  BufferType Type {BufferType::None};
  VertexBufferDesc VertexDesc {};
  IndexBufferDesc IndexDesc {};
  ConstantBufferDesc ConstantDesc {};
  StructuredBufferDesc StructuredDesc {};
  Shared<void> Data {nullptr};

  [[nodiscard]] bool IsValid() const noexcept
  {
    return Type != BufferType::None && Data != nullptr;
  }
  explicit operator bool() const noexcept
  {
    return IsValid();
  }
};

// Texture descriptor & object ---------------------------------------

/// Description of a texture allocation.
struct TextureDesc
{
  u32 Width {0};
  u32 Height {0};
  u32 Depth {1};
  u32 NumMips {1};
  u32 NumArraySlices {1};
  DataFormat Format {DataFormat::None};
  TextureType Type {TextureType::None};
  MemoryType Memory {MemoryType::None};
  bool IsRenderTarget {false};
  bool IsDepthStencil {false};
  bool AllowUnorderedAccess {false};
  ClearValue OptimizedClear {};
  const char* DebugName {nullptr};

  [[nodiscard]] bool IsValid() const noexcept
  {
    return Width > 0 && Height > 0 && Format != DataFormat::None && Type != TextureType::None &&
           Memory != MemoryType::None;
  }
  explicit operator bool() const noexcept
  {
    return IsValid();
  }
};

/// Opaque texture handle. The owning device must outlive every `Texture`.
struct Texture
{
  TextureDesc Desc {};
  Shared<void> Data {nullptr};

  [[nodiscard]] bool IsValid() const noexcept
  {
    return Desc.IsValid() && Data != nullptr;
  }
  explicit operator bool() const noexcept
  {
    return IsValid();
  }
};

// RenderTarget descriptor & object ----------------------------------

/// Description of a single render-target attachment: either one color
/// or one depth-stencil image. To bind multiple color targets in a
/// pass, create one `RenderTarget` per color and pass them all to
/// `BeginRenderingInfo::Colors`.
struct RenderTargetDesc
{
  /// Maximum simultaneously-bound color render targets per pass /
  /// per graphics pipeline. Kept here as the natural home for the
  /// engine-wide MRT cap.
  static constexpr u32 MaxRenderTargets = 8;

  u32 Width {0};
  u32 Height {0};
  /// Attachment format. If `IsDepthFormat(Format)` is true this is a
  /// depth-stencil target; otherwise a color target.
  DataFormat Format {DataFormat::None};
  /// Optimised clear value baked into the underlying texture. For depth
  /// targets the depth/stencil fields are used.
  ClearValue Clear {};
  const char* DebugName {nullptr};

  [[nodiscard]] bool IsDepth() const noexcept
  {
    return IsDepthFormat(Format);
  }
  [[nodiscard]] bool IsValid() const noexcept
  {
    return Width != 0 && Height != 0 && Format != DataFormat::None;
  }
  explicit operator bool() const noexcept
  {
    return IsValid();
  }
};

/// Opaque render-target handle. Owns the single backing texture (color
/// or depth-stencil). The owning device must outlive it.
struct RenderTarget
{
  RenderTargetDesc Desc {};
  /// Backing texture. Always valid when `IsValid()` returns true; can
  /// be sampled in subsequent passes (color targets are transitioned to
  /// SHADER_READ_ONLY automatically by `EndRendering`).
  Texture BackingTexture {};
  Shared<void> Data {nullptr};

  [[nodiscard]] bool IsValid() const noexcept
  {
    return Desc.IsValid() && Data != nullptr;
  }
  explicit operator bool() const noexcept
  {
    return IsValid();
  }
};

// Command list interface ------------------------------------------------
// TODO: this is the only render struct that is used by the command list interface, this will have to be refactored and
// moved to a more appropriate place when more like these are added.

struct BeginRenderingInfo
{
  Span<const RenderTarget> Colors {};
  Span<const ClearValue> ClearColors {};
  const RenderTarget* Depth {nullptr};
  const ClearValue* DepthClear {nullptr};
};

// Shader code -------------------------------------------------------

/// Shader-source reference. Either points to inline `Bytes` (e.g. via
/// `#embed`) or to an on-disk `Path`. `Entry` names the entry point.
struct ShaderCode
{
  ShaderFormat Format {ShaderFormat::None};
  ::std::span<const ::gecko::byte> Bytes {};  ///< inline (e.g. #embed)
  const char* Path {nullptr};                 ///< on-disk fallback
  const char* Entry {"main"};

  [[nodiscard]] bool IsValid() const noexcept
  {
    return Format != ShaderFormat::None && (!Bytes.empty() || Path != nullptr);
  }
};

// Pipeline descriptors & objects ------------------------------------

/// Description of a graphics pipeline (vertex + pixel shader).
struct GraphicsPipelineDesc
{
  ShaderCode VertexShader {};
  ShaderCode PixelShader {};

  VertexLayout Layout {};

  u32 NumRenderTargets {0};
  DataFormat RenderTargetFormats[RenderTargetDesc::MaxRenderTargets] {DataFormat::None};
  DataFormat DepthStencilFormat {DataFormat::None};

  static constexpr u32 MaxPipelineResources = 32;
  PipelineResource PipelineResources[MaxPipelineResources] {};
  u32 NumPipelineResources {0};

  /// Size in bytes of the push-constant block visible to all stages.
  /// Must be a multiple of 4 and <= `MaxPushConstantBytes`.
  u32 PushConstantBytes {0};

  CullMode Culling {CullMode::None};
  WindingOrder Winding {WindingOrder::ClockWise};
  PrimitiveType Primitive {PrimitiveType::Triangles};

  DepthStencilState DepthStencil {};
  RenderTargetBlendState BlendStates[RenderTargetDesc::MaxRenderTargets] {};

  const char* DebugName {nullptr};

  [[nodiscard]] bool IsValid() const noexcept
  {
    if (!VertexShader.IsValid())
      return false;
    if (NumRenderTargets == 0 && DepthStencilFormat == DataFormat::None)
      return false;
    for (u32 i = 0; i < NumRenderTargets; ++i)
    {
      if (RenderTargetFormats[i] == DataFormat::None)
        return false;
    }
    if (PushConstantBytes > MaxPushConstantBytes || (PushConstantBytes % 4) != 0)
      return false;
    return true;
  }
  explicit operator bool() const noexcept
  {
    return IsValid();
  }
};

/// Opaque graphics-pipeline handle. The owning device must outlive it.
struct GraphicsPipeline
{
  GraphicsPipelineDesc Desc {};
  Shared<void> Data {nullptr};

  [[nodiscard]] bool IsValid() const noexcept
  {
    return Desc.IsValid() && Data != nullptr;
  }
  explicit operator bool() const noexcept
  {
    return IsValid();
  }
};

/// Description of a compute pipeline.
struct ComputePipelineDesc
{
  ShaderCode ComputeShader {};

  static constexpr u32 MaxPipelineResources = 48;
  PipelineResource PipelineResources[MaxPipelineResources] {};
  u32 NumPipelineResources {0};

  /// Size in bytes of the push-constant block. Must be a multiple of 4
  /// and <= `MaxPushConstantBytes`.
  u32 PushConstantBytes {0};

  const char* DebugName {nullptr};

  [[nodiscard]] bool IsValid() const noexcept
  {
    if (!ComputeShader.IsValid())
      return false;
    if (PushConstantBytes > MaxPushConstantBytes || (PushConstantBytes % 4) != 0)
      return false;
    return true;
  }
  explicit operator bool() const noexcept
  {
    return IsValid();
  }
};

/// Opaque compute-pipeline handle. The owning device must outlive it.
struct ComputePipeline
{
  ComputePipelineDesc Desc {};
  Shared<void> Data {nullptr};

  [[nodiscard]] bool IsValid() const noexcept
  {
    return Desc.IsValid() && Data != nullptr;
  }
  explicit operator bool() const noexcept
  {
    return IsValid();
  }
};

// Swapchain descriptor & object -------------------------------------

/// Description of a swapchain attached to a window.
struct SwapchainDesc
{
  u32 Width {0};
  u32 Height {0};
  u32 NumBackBuffers {2};
  DataFormat Format {DataFormat::R8G8B8A8_UNORM};
  bool VSync {true};
  const char* DebugName {nullptr};

  [[nodiscard]] bool IsValid() const noexcept
  {
    return Width > 0 && Height > 0 && NumBackBuffers > 0 && Format != DataFormat::None;
  }
  explicit operator bool() const noexcept
  {
    return IsValid();
  }
};

struct Swapchain
{
  SwapchainDesc Desc {};
  Shared<void> Data {nullptr};

  Swapchain() = default;
  ~Swapchain() = default;

  Swapchain(const Swapchain&) = delete;
  Swapchain& operator=(const Swapchain&) = delete;

  Swapchain(Swapchain&&) noexcept = default;
  Swapchain& operator=(Swapchain&&) noexcept = default;

  [[nodiscard]] bool IsValid() const noexcept
  {
    return Desc.IsValid() && Data != nullptr;
  }
  explicit operator bool() const noexcept
  {
    return IsValid();
  }
};

// Query pool (timestamps) -------------------------------------------

/// Description of a timestamp query pool.
struct QueryPoolDesc
{
  u32 Count {0};
  const char* DebugName {nullptr};

  [[nodiscard]] bool IsValid() const noexcept
  {
    return Count > 0;
  }
};

/// Opaque query-pool handle. The owning device must outlive it.
struct QueryPool
{
  QueryPoolDesc Desc {};
  Shared<void> Data {nullptr};

  [[nodiscard]] bool IsValid() const noexcept
  {
    return Desc.IsValid() && Data != nullptr;
  }
  explicit operator bool() const noexcept
  {
    return IsValid();
  }
};

}  // namespace gecko::graphics
