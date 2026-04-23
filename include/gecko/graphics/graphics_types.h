#pragma once

#include "gecko/core/ptr.h"
#include "gecko/core/types.h"

#include <span>

namespace gecko::graphics {

// ── Named constants ───────────────────────────────────────────────────────

inline constexpr u32 MaxSwapchainImages     = 8;
inline constexpr u32 MaxFramesInFlight      = 2;
inline constexpr u32 MaxSwapchainsPerSubmit = 4;
inline constexpr u32 MaxPushConstantBytes   = 128;

// ── Enums ─────────────────────────────────────────────────────────────────

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

enum class ShaderFormat : u8
{
  None,
  SPIRV,        ///< Vulkan native; portable for runtime translation
  DXIL,         ///< DX12 native
  GLSL_Source,  ///< runtime-compiled (future)
  HLSL_Source,  ///< runtime-compiled (future)
};

enum class ClearValueType : u8
{
  RenderTarget,
  DepthStencil,
};

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

enum class CullMode : u8
{
  None,
  Back,
  Front,
};

enum class WindingOrder : u8
{
  ClockWise,
  CounterClockWise,
};

enum class PrimitiveType : u8
{
  Lines,
  Triangles,
};

enum class SamplerFilter : u8
{
  Linear,
  Point,
};

enum class SamplerWrapMode : u8
{
  Wrap,
  Clamp,
};

enum class ResourceType : u8
{
  None,
  Texture,
  RWTexture,
  ConstantBuffer,
  StructuredBuffer,
  RWStructuredBuffer,
  Sampler,
  LocalData,
};

enum class BufferType : u8
{
  None,
  Vertex,
  Index,
  Constant,
  Structured,
};

enum class MemoryType : u8
{
  None,
  Shared,
  Dedicated,
};

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

enum class BlendOp : u8
{
  Add,
  Subtract,
  ReverseSubtract,
  Min,
  Max,
};

enum class ColorWriteMask : u8
{
  None  = 0,
  Red   = 1 << 0,
  Green = 1 << 1,
  Blue  = 1 << 2,
  Alpha = 1 << 3,
  All   = Red | Green | Blue | Alpha,
};

// ── Utility functions ─────────────────────────────────────────────────────

[[nodiscard]] constexpr u32 FormatSizeInBytes(DataFormat format) noexcept
{
  switch (format)
  {
    case DataFormat::R8G8B8A8_SRGB:
    case DataFormat::R8G8B8A8_UNORM:
    case DataFormat::B8G8R8A8_UNORM:
    case DataFormat::B8G8R8A8_SRGB: return 4;
    case DataFormat::R32G32_FLOAT: return 8;
    case DataFormat::R32G32B32_FLOAT: return 12;
    case DataFormat::R32G32B32A32_FLOAT: return 16;
    case DataFormat::R16G16B16A16_FLOAT: return 8;
    case DataFormat::R32_FLOAT: return 4;
    case DataFormat::R8_UINT:
    case DataFormat::R8_INT: return 1;
    case DataFormat::R16_UINT:
    case DataFormat::R16_INT: return 2;
    case DataFormat::R32_UINT:
    case DataFormat::R32_INT: return 4;
    case DataFormat::D32_FLOAT: return 4;
    case DataFormat::D24_UNORM_S8_UINT: return 4;
    case DataFormat::D16_UNORM: return 2;
    default: return 0;
  }
}

[[nodiscard]] constexpr bool IsDepthFormat(DataFormat format) noexcept
{
  switch (format)
  {
    case DataFormat::D32_FLOAT:
    case DataFormat::D24_UNORM_S8_UINT:
    case DataFormat::D16_UNORM: return true;
    default: return false;
  }
}

[[nodiscard]] constexpr u32 CalculateNumberOfMips(u32 width,
                                                   u32 height) noexcept
{
  if (width == 0 || height == 0)
    return 0;
  u32 mips = 1;
  while (width > 1 || height > 1)
  {
    width  = (width > 1) ? width / 2 : 1;
    height = (height > 1) ? height / 2 : 1;
    ++mips;
  }
  return mips;
}

// ── Descriptor structs ────────────────────────────────────────────────────

struct VertexAttribute
{
  const char* Name {"VertexAttribute"};
  DataFormat  AttributeFormat {DataFormat::None};
  u32         Size {0};
  u32         Offset {0};

  VertexAttribute() = default;

  VertexAttribute(DataFormat format, const char* name) noexcept
      : Name(name),
        AttributeFormat(format),
        Size(FormatSizeInBytes(format)),
        Offset(0)
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
    return AttributeFormat == other.AttributeFormat && Size == other.Size
           && Offset == other.Offset;
  }
};

struct VertexLayout
{
  static constexpr u32 MaxAttributes = 16;

  VertexAttribute Attributes[MaxAttributes] {};
  u32             NumAttributes {0};
  u32             StrideInBytes {0};

  VertexLayout() = default;

  void AddAttribute(DataFormat format, const char* name) noexcept
  {
    if (NumAttributes >= MaxAttributes)
      return;
    const u32 size = FormatSizeInBytes(format);
    if (size == 0)
      return;
    VertexAttribute& attr = Attributes[NumAttributes];
    attr.Name             = name;
    attr.AttributeFormat  = format;
    attr.Size             = size;
    attr.Offset           = StrideInBytes;
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

struct ClearValue
{
  ClearValueType Type {ClearValueType::RenderTarget};
  f32            Color[4] {0.0F, 0.0F, 0.0F, 1.0F};
  f32            Depth {1.0F};
  u8             Stencil {0};

  [[nodiscard]] static constexpr ClearValue RenderTarget(f32 r, f32 g, f32 b,
                                                          f32 a) noexcept
  {
    ClearValue cv;
    cv.Type     = ClearValueType::RenderTarget;
    cv.Color[0] = r;
    cv.Color[1] = g;
    cv.Color[2] = b;
    cv.Color[3] = a;
    return cv;
  }

  [[nodiscard]] static constexpr ClearValue DepthStencil(f32 depth,
                                                          u8 stencil) noexcept
  {
    ClearValue cv;
    cv.Type    = ClearValueType::DepthStencil;
    cv.Depth   = depth;
    cv.Stencil = stencil;
    return cv;
  }
};

struct SamplerDesc
{
  SamplerFilter   Filter {SamplerFilter::Linear};
  SamplerWrapMode WrapMode {SamplerWrapMode::Wrap};
  const char*     DebugName {nullptr};

  [[nodiscard]] bool IsValid() const noexcept
  {
    return true;
  }
};

struct Sampler
{
  SamplerDesc  Desc {};
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

struct PipelineResource
{
  ResourceType Type {ResourceType::None};
  ShaderType   ShaderVisibility {ShaderType::All};
  u32          NumResources {1};

  [[nodiscard]] static constexpr PipelineResource TextureBinding(
      u32 count, ShaderType visibility) noexcept
  {
    return PipelineResource{
        .Type             = ResourceType::Texture,
        .ShaderVisibility = visibility,
        .NumResources     = count,
    };
  }

  [[nodiscard]] static constexpr PipelineResource RWTextureBinding(
      u32 count, ShaderType visibility) noexcept
  {
    return PipelineResource{
        .Type             = ResourceType::RWTexture,
        .ShaderVisibility = visibility,
        .NumResources     = count,
    };
  }

  [[nodiscard]] static constexpr PipelineResource ConstantBufferBinding(
      u32 count, ShaderType visibility) noexcept
  {
    return PipelineResource{
        .Type             = ResourceType::ConstantBuffer,
        .ShaderVisibility = visibility,
        .NumResources     = count,
    };
  }

  [[nodiscard]] static constexpr PipelineResource StructuredBufferBinding(
      u32 count, ShaderType visibility) noexcept
  {
    return PipelineResource{
        .Type             = ResourceType::StructuredBuffer,
        .ShaderVisibility = visibility,
        .NumResources     = count,
    };
  }

  [[nodiscard]] static constexpr PipelineResource RWStructuredBufferBinding(
      u32 count, ShaderType visibility) noexcept
  {
    return PipelineResource{
        .Type             = ResourceType::RWStructuredBuffer,
        .ShaderVisibility = visibility,
        .NumResources     = count,
    };
  }

  [[nodiscard]] static constexpr PipelineResource SamplerBinding(
      u32 count, ShaderType visibility) noexcept
  {
    return PipelineResource{
        .Type             = ResourceType::Sampler,
        .ShaderVisibility = visibility,
        .NumResources     = count,
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

struct StencilOpDesc
{
  StencilOp   Fail {StencilOp::Keep};
  StencilOp   DepthFail {StencilOp::Keep};
  StencilOp   Pass {StencilOp::Keep};
  CompareFunc Compare {CompareFunc::Always};
};

struct DepthStencilState
{
  bool          DepthTestEnable {false};
  bool          DepthWriteEnable {false};
  CompareFunc   DepthCompare {CompareFunc::Less};
  bool          StencilEnable {false};
  u8            StencilReadMask {0xFF};
  u8            StencilWriteMask {0xFF};
  StencilOpDesc StencilFront {};
  StencilOpDesc StencilBack {};
};

struct RenderTargetBlendState
{
  bool        BlendEnable {false};
  BlendFactor SrcColor {BlendFactor::One};
  BlendFactor DstColor {BlendFactor::Zero};
  BlendOp     ColorOp {BlendOp::Add};
  BlendFactor SrcAlpha {BlendFactor::One};
  BlendFactor DstAlpha {BlendFactor::Zero};
  BlendOp     AlphaOp {BlendOp::Add};
  u8          WriteMask {static_cast<u8>(ColorWriteMask::All)};
};

// ── Buffer descriptors & object ───────────────────────────────────────────

struct VertexBufferDesc
{
  u32         NumVertices {0};
  u32         VertexSize {0};
  MemoryType  Memory {MemoryType::None};
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

struct IndexBufferDesc
{
  u32         NumIndices {0};
  MemoryType  Memory {MemoryType::None};
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

struct ConstantBufferDesc
{
  u32         SizeInBytes {0};
  MemoryType  Memory {MemoryType::None};
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

struct StructuredBufferDesc
{
  u32         NumElements {0};
  u32         ElementSize {0};
  MemoryType  Memory {MemoryType::None};
  bool        AllowUnorderedAccess {false};
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

struct Buffer
{
  BufferType        Type {BufferType::None};
  VertexBufferDesc  VertexDesc {};
  IndexBufferDesc   IndexDesc {};
  ConstantBufferDesc ConstantDesc {};
  StructuredBufferDesc StructuredDesc {};
  Shared<void>      Data {nullptr};

  [[nodiscard]] bool IsValid() const noexcept
  {
    return Type != BufferType::None && Data != nullptr;
  }
  explicit operator bool() const noexcept
  {
    return IsValid();
  }
};

// ── Texture descriptor & object ───────────────────────────────────────────

struct TextureDesc
{
  u32         Width {0};
  u32         Height {0};
  u32         Depth {1};
  u32         NumMips {1};
  u32         NumArraySlices {1};
  DataFormat  Format {DataFormat::None};
  TextureType Type {TextureType::None};
  MemoryType  Memory {MemoryType::None};
  bool        IsRenderTarget {false};
  bool        IsDepthStencil {false};
  bool        AllowUnorderedAccess {false};
  ClearValue  OptimizedClear {};
  const char* DebugName {nullptr};

  [[nodiscard]] bool IsValid() const noexcept
  {
    return Width > 0 && Height > 0 && Format != DataFormat::None
           && Type != TextureType::None && Memory != MemoryType::None;
  }
  explicit operator bool() const noexcept
  {
    return IsValid();
  }
};

struct Texture
{
  TextureDesc  Desc {};
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

// ── RenderTarget descriptor & object ──────────────────────────────────────

struct RenderTargetDesc
{
  static constexpr u32 MaxRenderTargets = 8;

  u32        Width {0};
  u32        Height {0};
  u32        NumRenderTargets {0};
  DataFormat RenderTargetFormats[MaxRenderTargets] {DataFormat::None};
  DataFormat DepthStencilFormat {DataFormat::None};
  ClearValue RenderTargetClearValues[MaxRenderTargets] {};
  ClearValue DepthStencilClearValue {ClearValue::DepthStencil(1.0F, 0)};
  const char* DebugName {nullptr};

  [[nodiscard]] bool IsValid() const noexcept
  {
    if (Width == 0 || Height == 0)
      return false;
    if (NumRenderTargets == 0 && DepthStencilFormat == DataFormat::None)
      return false;
    if (NumRenderTargets > MaxRenderTargets)
      return false;
    for (u32 i = 0; i < NumRenderTargets; ++i)
    {
      if (RenderTargetFormats[i] == DataFormat::None)
        return false;
    }
    return true;
  }
  explicit operator bool() const noexcept
  {
    return IsValid();
  }
};

struct RenderTarget
{
  RenderTargetDesc Desc {};
  Texture          RenderTextures[RenderTargetDesc::MaxRenderTargets] {};
  Texture          DepthTexture {};
  Shared<void>     Data {nullptr};

  [[nodiscard]] bool IsValid() const noexcept
  {
    return Desc.IsValid() && Data != nullptr;
  }
  explicit operator bool() const noexcept
  {
    return IsValid();
  }
};

// ── Shader code ───────────────────────────────────────────────────────────

struct ShaderCode
{
  ShaderFormat                     Format {ShaderFormat::None};
  ::std::span<const ::gecko::byte> Bytes {};           ///< inline (e.g. #embed)
  const char*                      Path {nullptr};    ///< on-disk fallback
  const char*                      Entry {"main"};

  [[nodiscard]] bool IsValid() const noexcept
  {
    return Format != ShaderFormat::None && (!Bytes.empty() || Path != nullptr);
  }
};

// ── Pipeline descriptors & objects ────────────────────────────────────────

struct GraphicsPipelineDesc
{
  ShaderCode VertexShader {};
  ShaderCode PixelShader {};

  VertexLayout Layout {};

  u32        NumRenderTargets {0};
  DataFormat RenderTargetFormats[RenderTargetDesc::MaxRenderTargets] {
      DataFormat::None};
  DataFormat DepthStencilFormat {DataFormat::None};

  static constexpr u32 MaxPipelineResources = 32;
  PipelineResource     PipelineResources[MaxPipelineResources] {};
  u32                  NumPipelineResources {0};

  /// Size in bytes of the push-constant block visible to all stages.
  /// Must be a multiple of 4 and ≤ `MaxPushConstantBytes`.
  u32 PushConstantBytes {0};

  CullMode      Culling {CullMode::None};
  WindingOrder  Winding {WindingOrder::ClockWise};
  PrimitiveType Primitive {PrimitiveType::Triangles};

  DepthStencilState      DepthStencil {};
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
    if (PushConstantBytes > MaxPushConstantBytes
        || (PushConstantBytes % 4) != 0)
      return false;
    return true;
  }
  explicit operator bool() const noexcept
  {
    return IsValid();
  }
};

struct GraphicsPipeline
{
  GraphicsPipelineDesc Desc {};
  Shared<void>         Data {nullptr};

  [[nodiscard]] bool IsValid() const noexcept
  {
    return Desc.IsValid() && Data != nullptr;
  }
  explicit operator bool() const noexcept
  {
    return IsValid();
  }
};

struct ComputePipelineDesc
{
  ShaderCode ComputeShader {};

  static constexpr u32 MaxPipelineResources = 48;
  PipelineResource     PipelineResources[MaxPipelineResources] {};
  u32                  NumPipelineResources {0};

  /// Size in bytes of the push-constant block. Must be a multiple of 4
  /// and ≤ `MaxPushConstantBytes`.
  u32 PushConstantBytes {0};

  const char* DebugName {nullptr};

  [[nodiscard]] bool IsValid() const noexcept
  {
    if (!ComputeShader.IsValid())
      return false;
    if (PushConstantBytes > MaxPushConstantBytes
        || (PushConstantBytes % 4) != 0)
      return false;
    return true;
  }
  explicit operator bool() const noexcept
  {
    return IsValid();
  }
};

struct ComputePipeline
{
  ComputePipelineDesc Desc {};
  Shared<void>        Data {nullptr};

  [[nodiscard]] bool IsValid() const noexcept
  {
    return Desc.IsValid() && Data != nullptr;
  }
  explicit operator bool() const noexcept
  {
    return IsValid();
  }
};

// ── Swapchain descriptor & object ─────────────────────────────────────────

struct SwapchainDesc
{
  u32         Width {0};
  u32         Height {0};
  u32         NumBackBuffers {2};
  DataFormat  Format {DataFormat::R8G8B8A8_UNORM};
  bool        VSync {true};
  const char* DebugName {nullptr};

  [[nodiscard]] bool IsValid() const noexcept
  {
    return Width > 0 && Height > 0 && NumBackBuffers > 0
           && Format != DataFormat::None;
  }
  explicit operator bool() const noexcept
  {
    return IsValid();
  }
};

struct Swapchain
{
  SwapchainDesc Desc {};
  Shared<void>  Data {nullptr};

  Swapchain()  = default;
  ~Swapchain() = default;

  Swapchain(const Swapchain&)            = delete;
  Swapchain& operator=(const Swapchain&) = delete;

  Swapchain(Swapchain&&) noexcept            = default;
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

// ── Query pool (timestamps) ───────────────────────────────────────────────

struct QueryPoolDesc
{
  u32         Count {0};
  const char* DebugName {nullptr};

  [[nodiscard]] bool IsValid() const noexcept
  {
    return Count > 0;
  }
};

struct QueryPool
{
  QueryPoolDesc Desc {};
  Shared<void>  Data {nullptr};

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
