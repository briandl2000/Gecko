#pragma once

#include "gecko/core/ptr.h"
#include "gecko/core/types.h"

#include <span>

namespace gecko::graphics {

// ── Named constants ───────────────────────────────────────────────────────

inline constexpr u32 MaxSwapchainImages     = 8;
inline constexpr u32 MaxFramesInFlight      = 2;
inline constexpr u32 MaxSwapchainsPerSubmit = 4;

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
  ConstantBuffer,
  StructuredBuffer,
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

  [[nodiscard]] bool IsValid() const noexcept
  {
    return true;
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

  [[nodiscard]] static constexpr PipelineResource ConstantBufferBinding(
      u32 count, ShaderType visibility) noexcept
  {
    return PipelineResource{
        .Type             = ResourceType::ConstantBuffer,
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

// ── Buffer descriptors & object ───────────────────────────────────────────

struct VertexBufferDesc
{
  u32        NumVertices {0};
  u32        VertexSize {0};
  MemoryType Memory {MemoryType::None};

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
  u32        NumIndices {0};
  MemoryType Memory {MemoryType::None};

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
  u32        SizeInBytes {0};
  MemoryType Memory {MemoryType::None};

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
  u32        NumElements {0};
  u32        ElementSize {0};
  MemoryType Memory {MemoryType::None};

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
  ClearValue  OptimizedClear {};

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

  static constexpr u32 MaxSamplers = 8;
  SamplerDesc          SamplerDescs[MaxSamplers] {};
  u32                  NumSamplers {0};

  CullMode      Culling {CullMode::None};
  WindingOrder  Winding {WindingOrder::ClockWise};
  PrimitiveType Primitive {PrimitiveType::Triangles};
  bool          DepthBoundsTest {false};

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

  static constexpr u32 MaxReadOnlyResources = 32;
  PipelineResource     ReadOnlyResources[MaxReadOnlyResources] {};
  u32                  NumReadOnlyResources {0};

  static constexpr u32 MaxReadWriteResources = 16;
  PipelineResource     ReadWriteResources[MaxReadWriteResources] {};
  u32                  NumReadWriteResources {0};

  static constexpr u32 MaxSamplers = 8;
  SamplerDesc          SamplerDescs[MaxSamplers] {};
  u32                  NumSamplers {0};

  [[nodiscard]] bool IsValid() const noexcept
  {
    return ComputeShader.IsValid();
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
  u32        Width {0};
  u32        Height {0};
  u32        NumBackBuffers {2};
  DataFormat Format {DataFormat::R8G8B8A8_UNORM};
  bool       VSync {true};

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

}  // namespace gecko::graphics
