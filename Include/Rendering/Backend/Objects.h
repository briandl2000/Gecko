#pragma once

#include "Defines.h"

#include <vector>

namespace Gecko
{
#pragma region enums
  /** @brief Shader types */
  enum class ShaderType : u8
  {
    All,
    Vertex,
    Pixel,
    Compute
  };

  /** @brief Data formats */
  enum class DataFormat : u16
  {
    /** @brief Default invalid data format */
    None,
    R8G8B8A8_SRGB,
    R8G8B8A8_UNORM,
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

  };

  /** @brief Clear value type  */
  enum class ClearValueType : u8
  {
    RenderTarget,
    DepthStencil
  };

  /** @brief Texture type */
  enum class TextureType : u8
  {
    /** @brief Default invalid texture type*/
    None,
    Tex1D,
    Tex2D,
    Tex3D,
    TexCube,
    Tex1DArray,
    Tex2DArray
  };

  /** @brief Cull mode */
  enum class CullMode : u8
  {
    None,
    Back,
    Front
  };

  /** @brief Winding order */
  enum class WindingOrder : u8
  {
    ClockWise,
    CounterClockWise
  };

  /** @brief Primitive type */
  enum class PrimitiveType : u8
  {
    Lines,
    Triangles
  };

  /** @brief Sampler filter */
  enum class SamplerFilter : u8
  {
    Linear,
    Point
  };

  /** @brief Sampler wrap mode */
  enum class SamplerWrapMode : u8
  {
    Wrap,
    Clamp
  };

  /** @brief Rampler type */
  enum class ResourceType : u8
  {
    /** @brief Default invalid resource type */
    None,
    Texture,
    ConstantBuffer,
    StructuredBuffer,
    LocalData
  };

  /** @brief Buffer type */
  enum class BufferType : u8
  {
    /** @brief Default invalid buffer type */
    None,
    Vertex,
    Index,
    Constant,
    Structured
  };

  /** @brief Memory type */
  enum class MemoryType : u8
  {
    /** @brief Default invalid memory type */
    None,
    Shared,
    Dedicated
  };

  // Helper functions

  std::string EnumToString(ShaderType val);
  std::string EnumToString(DataFormat val);
  std::string EnumToString(ClearValueType val);
  std::string EnumToString(TextureType val);
  std::string EnumToString(CullMode val);
  std::string EnumToString(WindingOrder val);
  std::string EnumToString(PrimitiveType val);
  std::string EnumToString(SamplerFilter val);
  std::string EnumToString(SamplerWrapMode val);
  std::string EnumToString(ResourceType val);
  std::string EnumToString(BufferType val);
  std::string EnumToString(MemoryType val);

  /**
   * @brief
   *
   * @param format
   * @return The size of the specified data format as a u32
   */
  u32 FormatSizeInBytes(const DataFormat& format);

  /**
   * @brief
   * A function to get the number of mips of a texture given its width and height
   * @param width The width of the texture in pixels
   * @param height The height of the texture in pixels
   * @return The number of maximal mips for a texture in u32
   */
  u32 CalculateNumberOfMips(u32 width, u32 height);
#pragma endregion

#pragma region structs
  // Information structs

  /**
   * @brief
   * Vertex attribute for creating vertex layouts
   */
  struct VertexAttribute
  {
    const char* Name{ "VertexAttribute" };
    DataFormat AttributeFormat{ DataFormat::None };
    u32 Size{ 0 };
    u32 Offset{ 0 };

    VertexAttribute() = default;
    VertexAttribute(DataFormat format, const char* name)
      : Name(name), AttributeFormat(format), Size(FormatSizeInBytes(format)), Offset(0)
    {}

    /**
     * @brief
     * @param failureReason Filled in with the reason the object is invalid
     * @return true if VertexAttribute is valid.
     * VertexAttribute is valid when:
     * AttributeFormat != None and Size > 0
     */
    bool IsValid(std::string* failureReason = nullptr) const;
    operator bool() const { return IsValid(); }

    // Does not check for name equality
    bool operator==(const VertexAttribute& other) const
    {
      return AttributeFormat == other.AttributeFormat && Size == other.Size && Offset == other.Offset;
    }
    bool operator!=(const VertexAttribute& other) const
    {
      return !(*this == other);
    }
  };

  /**
   * @brief
   * Vertex layout used for vertex and pipeline creation
   */
  struct VertexLayout
  {
    std::vector<VertexAttribute> Attributes{};
    u32 Stride{ 0 };

    VertexLayout() = default;
    VertexLayout(const std::initializer_list<VertexAttribute> attributes)
      : Attributes(attributes)
    {
      CalculateOffsetsAndStride();
    }
    VertexLayout(const std::vector<VertexAttribute> attributes)
      : Attributes(attributes)
    {
      CalculateOffsetsAndStride();
    }
    ~VertexLayout() {}

    /**
     * @brief
     * @param failureReason Filled in with the reason the object is invalid
     * @return true if VertexLayout is valid.
     * VertexLayout is valid when:
     * All vertex attributes are valid
     */
    bool IsValid(std::string* failureReason = nullptr) const;
    operator bool() const { return IsValid(); }

    bool operator==(const VertexLayout& other) const
    {
      if (Stride != other.Stride || Attributes.size() != other.Attributes.size())
      {
        return false;
      }
      for (size_t i = 0; i < Attributes.size(); ++i)
      {
        if (Attributes[i] != other.Attributes[i])
        {
          return false;
        }
      }
      return true;
    }
    bool operator!=(const VertexLayout& other) const
    {
      return !(*this == other);
    }

  private:
    void CalculateOffsetsAndStride()
    {
      u32 offset = 0;
      Stride = 0;
      for (VertexAttribute& attributte : Attributes)
      {
        attributte.Offset = offset;
        offset += attributte.Size;
        Stride += attributte.Size;
      }
    }
  };

  /**
   * @brief
   * Clear values used for render target creation
   */
  struct ClearValue
  {
    union
    {
      f32 Values[4]{ 0.f };
      f32 Depth;
    };

    ClearValue(ClearValueType type = ClearValueType::RenderTarget)
    {
      switch (type)
      {
      case ClearValueType::RenderTarget:
        Values[0] = Values[1] = Values[2] = 0.f;
        Values[3] = 1.f;
        break;
      case ClearValueType::DepthStencil:
        Depth = 1.f;
        break;
      }
    }
    ClearValue(f32 r, f32 g, f32 b, f32 a)
    {
      Values[0] = r;
      Values[1] = g;
      Values[2] = b;
      Values[3] = a;
    }

    bool IsValid(std::string* failureReason = nullptr) const;
    operator bool() const { return IsValid(); }

    bool operator==(const ClearValue& other) const
    {
      for (u32 i = 0; i < 4; ++i)
      {
        if (Values[i] != other.Values[i])
          return false;
      }

      return true;
    }
    bool operator!=(const ClearValue& other) const
    {
      return !(*this == other);
    }
  };

  /**
   * @brief
   * Sampler descriptor used for pipeline creation
   */
  struct SamplerDesc
  {
    ShaderType Visibility{ ShaderType::All };
    SamplerFilter Filter{ SamplerFilter::Linear };
    SamplerWrapMode WrapU{ SamplerWrapMode::Wrap };
    SamplerWrapMode WrapV{ SamplerWrapMode::Wrap };
    SamplerWrapMode WrapW{ SamplerWrapMode::Wrap };

    bool IsValid(std::string* failureReason = nullptr) const;
    operator bool() const { return IsValid(); }

    bool operator==(const SamplerDesc& other) const
    {
      return Visibility == other.Visibility && Filter == other.Filter &&
        WrapU == other.WrapU && WrapV == other.WrapV && WrapW == other.WrapW;
    }
    bool operator!=(const SamplerDesc& other) const
    {
      return !(*this == other);
    }
  };

  /**
   * @brief
   * Pipeline resource used for pipeline creation
   */
  struct PipelineResource
  {
    /**
     * @brief
     *
     * @param visibility The shader visibility of this resource
     * @param bindLocation The texture bind location of this resource. this reflects the "tX" register in hlsl.
     * If it is used as a read write resource it reflects the "uX" register slot.
     * @return A texture PipelineResource
     */
    static PipelineResource Texture(ShaderType visibility, u32 bindLocation)
    {
      return { ResourceType::Texture, visibility, bindLocation, 0 };
    }
    /**
     * @brief
     *
     * @param visibility The shader visibility of this resource
     * @param bindLocation The constant buffer bind location of this resource. this reflects the "bX" register in hlsl.
     * @return A constant buffer PipelineResource
     */
    static PipelineResource ConstantBuffer(ShaderType visibility, u32 bindLocation)
    {
      return { ResourceType::ConstantBuffer, visibility, bindLocation, 0 };
    }
    /**
     * @brief
     *
     * @param visibility The shader visibility of this resource
     * @param bindLocation The texture bind location of this resource. this reflects the "tX" register in hlsl.
     * If it is used as a read write resource it reflects the "uX" register slot.
     * (Structured buffers are bound in texture registers)
     * @return A structured buffer PipelineResource
     */
    static PipelineResource StructuredBuffer(ShaderType visibility, u32 bindLocation)
    {
      return { ResourceType::StructuredBuffer, visibility, bindLocation, 0 };
    }
    /**
     * @brief
     * Local data is used for per call information within pipelines.
     * @param visibility The shader visibility of this resource
     * @param bindLocation The constant buffer bind location of this resource. this reflects the "bX" register in hlsl.
     * (You can have only one local data resource per pipeline)
     * @return A texture PipelineResource
     */
    static PipelineResource LocalData(ShaderType visibility, u32 bindLocation, u32 size)
    {
      return { ResourceType::LocalData, visibility, bindLocation, size };
    }

    ResourceType Type{ ResourceType::None };
    ShaderType Visibility{ ShaderType::All };
    u32 BindLocation{ 0 };
    u32 Size{ 0 };

    /**
     * @brief
     * @param failureReason Filled in with the reason the object is invalid
     * @return true PipelineResource is valid.
     * PipelineResource is valid when:
     * Type != None,
     * When Type == LocalData Size can't be 0 and Size needs to be a multiple of 4
     */
    bool IsValid(std::string* failureReason = nullptr) const;
    operator bool() const { return IsValid(); }
  };

  // Buffers

  /*
   * There are 4 different buffer Types each buffer has different properties
   * Vertex buffers are used for vertex data in graphics pipelines
   * Index buffers are used for index data in graphics pipelines
   * Constant buffers are used for any type of data that can not be changed and has one
   *  specific size in graphics or compute pipelines
   * Structured buffers are arrays of structs that can be used in graphics or compute pipelines
   *  you can write to structured bufferes within a compute pipeline
   * !!Important!! When binding a buffer as read write it needs to be created in dedicated memory!
   *
   *                                      | Vertex | Index | Constant | Structured |
   * -------------------------------------------------------------------------------
   * Can be bound as Vertex Buffer        | X      |       |          |            |
   * -------------------------------------------------------------------------------
   * Can be bound as Index Buffer         |        | X     |          |            |
   * -------------------------------------------------------------------------------
   * Can be bound as Constant Buffer      |        |       | X        |            |
   * -------------------------------------------------------------------------------
   * Can be bound as Structured Buffer    | X      | X     |          | X          |
   * -------------------------------------------------------------------------------
   * Can be bound as Read Write Buffer    | X      | X     |          | X          |
   * -------------------------------------------------------------------------------
   */

   /**
    * @brief
    * Vertex buffer descriptor used for creating a Buffer of BufferType::Vertex
    */
  struct VertexBufferDesc
  {
    VertexLayout Layout{ VertexLayout() };
    u32 NumVertices{ 0 };
    MemoryType MemoryType{ MemoryType::None };
    /** @brief Specifies if you can read and write to the buffer within a compute pipeline */
    bool CanReadWrite{ false };

    /**
     * @brief
     * @param failureReason Filled in with the reason the object is invalid
     * @return true if VertexBufferDesc is valid.
     * VertexBufferDesc is valid when:
     * Layout.Attributes.Size() > 0,
     * Layout is valid,
     * NumVertices > 0.
     */
    bool IsValid(std::string* failureReason = nullptr) const;
    operator bool() const { return IsValid(); }
  };

  /**
   * @brief
   * Index buffer descriptor used for creating a Buffer of BufferType::Index
   */
  struct IndexBufferDesc
  {
    DataFormat IndexFormat{ DataFormat::None };
    u32 NumIndices{ 0 };
    MemoryType MemoryType{ MemoryType::None };
    /** @brief Specifies if you can read and write to the buffer within a compute pipeline */
    bool CanReadWrite{ false };

    /**
     * @brief
     * @param failureReason Filled in with the reason the object is invalid
     * @return true if IndexBufferDesc is valid.
     * IndexBufferDesc is valid when:
     * IndexFormat == R16_UINT or IndexFormat == R32_UINT,
     * NumIndices > 0
     */
    bool IsValid(std::string* failureReason = nullptr) const;
    operator bool() const { return IsValid(); }
  };

  /**
   * @brief
   * Constant buffer descriptor used for creating a Buffer of BufferType::Constant
   */
  struct ConstantBufferDesc
  {
    u32 Size{ 0 };
    MemoryType MemoryType{ MemoryType::None };

    /**
     * @brief
     * @param failureReason Filled in with the reason the object is invalid
     * @return true if ConstantBufferDesc is valid.
     * ConstantBufferDesc is valid when:
     * Size > 0.
     */
    bool IsValid(std::string* failureReason = nullptr) const;
    operator bool() const { return IsValid(); }
  };

  /**
   * @brief
   * Structured buffer descriptor used for creating a Buffer of BufferType::Structured
   */
  struct StructuredBufferDesc
  {
    u32 StructSize{ 0 };
    u32 NumElements{ 0 };
    MemoryType MemoryType{ MemoryType::None };
    /** @brief Specifies if you can read and write to the buffer within a compute pipeline */
    bool CanReadWrite{ false };

    /**
     * @brief
     * @param failureReason Filled in with the reason the object is invalid
     * @return true if StructuredBufferDesc is valid.
     * StructuredBufferDesc is valid when:
     * StructSize > 0,
     * NumElements > 0.
     */
    bool IsValid(std::string* failureReason = nullptr) const;
    operator bool() const { return IsValid(); }
  };

  /**
   * @brief
   * Buffer descriptor used for creating a Buffer, this is auto generated from
   * the different buffer creation functions on the Device.
   */
  struct BufferDesc
  {
    BufferType Type{ BufferType::None };
    MemoryType MemoryType{ MemoryType::None };
    u32 Size{ 0 };
    u32 NumElements{ 0 };
    u32 Stride{ 0 };
    /** @brief Specifies if you can read and write to the buffer within a compute pipeline */
    bool CanReadWrite{ false };

    /**
     * @brief
     * @param failureReason Filled in with the reason the object is invalid
     * @return true if BufferDesc is valid.
     * BufferDesc is valid when:
     * MemoryType != None and Type != None,
     * if MemoryType == Shared CanReadWrite must be false,
     * if MemoryType == Constant CanReadWrite must be false and Size > 0,
     * otherwise Stride > 0 and NumElements > 0
     */
    bool IsValid(std::string* failureReason = nullptr) const;
    operator bool() { return IsValid(); }
  };

  /**
   * @brief
   * This is a buffer object. It can be used as either a Vertex, Index, Constant or Structured buffer.
   * Buffers are used to bind during a graphics or compute pipeline.
   */
  struct Buffer
  {
    const BufferDesc Desc{};
    /** @brief A shared void pointer to API specific data */
    Ref<void> Data{ nullptr };

    Buffer() = default;
    Buffer(const BufferDesc& desc)
      : Desc(desc)
    {}
    Buffer(const Buffer& other)
      : Desc(other.Desc), Data(other.Data)
    {}
    Buffer& operator=(const Buffer& other)
    {
      this->~Buffer();
      Buffer* buffer = new (this) Buffer(other);
      return *buffer;
    }

    /**
     * @brief
     * @param failureReason Filled in with the reason the object is invalid
     * @return true if Buffer is valid.
     * Buffer is valid when:
     * Desc is valid and Data != nullptr
     */
    bool IsValid(std::string* failureReason = nullptr) const;
    operator bool() const { return IsValid(); }
  };

  // Texture

  /**
   * @brief
   * Texture descriptor used for creating a Texture object
   */
  struct TextureDesc
  {
    DataFormat Format{ DataFormat::None };
    u32 Width{ 1 };
    u32 Height{ 1 };
    u32 Depth{ 1 };
    u32 NumMips{ 1 };
    u32 NumArraySlices{ 1 };
    TextureType Type{ TextureType::None };

    /**
     * @brief
     * @param failureReason Filled in with the reason the object is invalid
     * @return true if TextureDesc is valid.
     * TextureDesc is valid when:
     * Format != DataFormat::None and Type != TextureType::None,
     * if Type == Tex1D or Tex1DArray Height and Depth must be 1.
     * if Type == Tex2D or Tex2dArray Depth must be 1.
     */
    bool IsValid(std::string* failureReason = nullptr) const;
    operator bool() const { return IsValid(); }
  };

  /**
   * @brief
   * This is a texture object. Textures are used to bind during a graphics or compute pipeline.
   */
  struct Texture
  {
    const TextureDesc Desc{};
    /** @brief A shared void pointer to API specific data */
    Ref<void> Data{ nullptr };

    Texture() = default;
    Texture(const TextureDesc& desc)
      : Desc(desc)
    {}
    Texture(const Texture& other)
      : Desc(other.Desc), Data(other.Data)
    {}
    Texture& operator=(const Texture& other)
    {
      this->~Texture();
      Texture* texture = new (this) Texture(other);
      return *texture;
    }

    /**
     * @brief
     * @param failureReason Filled in with the reason the object is invalid
     * @return true if Texture is valid.
     * Texture is valid when:
     * Desc is valid and Data != nullptr
     */
    bool IsValid(std::string* failureReason = nullptr) const;
    operator bool() const { return IsValid(); }
  };

  // Render target

  /**
   * @brief
   * Render target descriptor used for creating a RenderTarget object
   */
  struct RenderTargetDesc
  {
    ClearValue RenderTargetClearValues[8]{ ClearValueType::RenderTarget };
    ClearValue DepthTargetClearValue{ ClearValueType::DepthStencil };
    DataFormat RenderTargetFormats[8]{
      DataFormat::None, DataFormat::None, DataFormat::None, DataFormat::None,
      DataFormat::None, DataFormat::None, DataFormat::None, DataFormat::None
    };
    DataFormat DepthStencilFormat{ DataFormat::None };
    u32 NumRenderTargets{ 0 };
    u32 NumMips[8]{ 1, 1, 1, 1, 1, 1, 1, 1 };
    u32 DepthMips{ 1 };
    u32 Width{ 0 };
    u32 Height{ 0 };

    /**
     * @brief
     * @param failureReason Filled in with the reason the object is invalid
     * @return true if RenderTargetDesc is valid.
     * RenderTargetDesc is valid when:
     * Width != 0 and Height != 0.
     * if NumRenderTargets > 0 RenderTargetFormats[0] to RenderTargetFormats[NumRenderTargets] cannot be None.
     * if no render targets are used DepthStencilFormat cannot be None.
     */
    bool IsValid(std::string* failureReason = nullptr) const;
    operator bool() const { return IsValid(); }
  };

  /**
   * @brief
   * This is a render target object. Render targets are used to bind during a graphics pipeline to render to.
   * A render target has an array of textures it uses for rendering and a depht texture.
   * You can use these textures like any other type of texture.
   */
  struct RenderTarget
  {
    const RenderTargetDesc Desc{};
    Texture RenderTextures[8]{
      Texture(), Texture(), Texture(), Texture(),
      Texture(), Texture(), Texture(), Texture() };
    Texture DepthTexture{ Texture() };
    /** @brief A shared void pointer to API specific data */
    Ref<void> Data{ nullptr };

    RenderTarget() = default;
    RenderTarget(const RenderTargetDesc& desc)
      : Desc(desc)
    {}
    RenderTarget(const RenderTarget& other)
      : Desc(other.Desc), Data(other.Data)
    {
      for (u32 i = 0; i < other.Desc.NumRenderTargets; i++)
      {
        RenderTextures[i] = other.RenderTextures[i];
      }
      DepthTexture = other.DepthTexture;
    }
    RenderTarget& operator=(const RenderTarget& other)
    {
      this->~RenderTarget();
      RenderTarget* renderTarget = new (this) RenderTarget(other);
      return *renderTarget;
    }

    /**
     * @brief
     * @param failureReason Filled in with the reason the object is invalid
     * @return true if RenderTarget is valid.
     * RenderTarget is valid when:
     * Desc is valid,
     * if Desc.NumRenderTargets > 0 RenderTextures[0] to RenderTextures[Desc.NumRenderTargets] are valid,
     * if Desc.DepthStencilFormat != None DepthTexture is valid,
     * Data != nullptr
     */
    bool IsValid(std::string* failureReason = nullptr) const;
    operator bool() const { return IsValid(); }
  };

  // Graphics pipeline

  /**
   * @brief
   * Graphics pipeline descriptor used for creating a GraphicsPipeline object
   */
  struct GraphicsPipelineDesc
  {
    const char* VertexShaderPath{ nullptr };
    /** @brief EntryPoint == the name of the entrypoint of the shader (DX12 allows for custom entrypoints), usually "main" */
    const char* VertexEntryPoint{ "main" };

    const char* PixelShaderPath{ nullptr };
    /** @brief EntryPoint == the name of the entrypoint of the shader (DX12 allows for custom entrypoints), usually "main" */
    const char* PixelEntryPoint{ "main" };

    /**
     * @brief
     * ShaderVersion == shader language version that the shader uses, formatted as "type_X_Y";
     * where type == vs for vertex shaders, ps for pixel shaders (type_ prefix gets added automatically in Device_DX12.cpp);
     * X == main version (e.g. 5 for hlsl shader language 5.1);
     * Y == subversion (e.g. 1 for 5.1);
     * Users should usually initialise this as pipelineDesc.ShaderVersion = "5_1" or something similar, unless they want to
     * use a shader type that has no support built into this library (so anything other than vertex, pixel or compute shaders).
     * See for example https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/specifying-compiler-targets for D3D12
     */
    const char* ShaderVersion{ nullptr };

    VertexLayout VertexLayout{};

    u32 NumRenderTargets{ 0 };
    DataFormat RenderTextureFormats[8]{ DataFormat::None };
    DataFormat DepthStencilFormat{ DataFormat::None };

    std::vector<PipelineResource> PipelineResources{};
    std::vector<SamplerDesc> SamplerDescs{};

    CullMode CullMode{ CullMode::None };
    WindingOrder WindingOrder{ WindingOrder::ClockWise };
    PrimitiveType PrimitiveType{ PrimitiveType::Triangles };
    bool DepthBoundsTest{ false };

    /**
     * @brief
     * @param failureReason Filled in with the reason the object is invalid
     * @return true if GraphicsPipelineDesc is valid.
     * GraphicsPipelineDesc is valid when:
     * VertexShaderPath != nullptr (atleast a vertex shader is needed for a graphics pipeline),
     * ShaderVersion != nullptr,
     * RenderTargetFormats[0] or DepthStencilFormat != None.
     */
    bool IsValid(std::string* failureReason = nullptr) const;
    operator bool() const { return IsValid(); }
  };

  /**
   * @brief
   * This is a graphics pipeline object. Graphics pipelines can be bound on a command buffer.
   */
  struct GraphicsPipeline
  {
    const GraphicsPipelineDesc Desc{};
    /** @brief A shared void pointer to API specific data */
    Ref<void> Data{ nullptr };

    GraphicsPipeline() = default;
    GraphicsPipeline(const GraphicsPipelineDesc& desc)
      : Desc(desc)
    {}
    GraphicsPipeline(const GraphicsPipeline& other)
      : Desc(other.Desc), Data(other.Data)
    {}
    GraphicsPipeline& operator=(const GraphicsPipeline& other)
    {
      this->~GraphicsPipeline();
      GraphicsPipeline* renderTarget = new (this) GraphicsPipeline(other);
      return *renderTarget;
    }

    /**
     * @brief
     * @param failureReason Filled in with the reason the object is invalid
     * @return true if GraphicsPipeline is valid.
     * GraphicsPipeline is valid when:
     * Desc is valid and Data != nullptr.
     */
    bool IsValid(std::string* failureReason = nullptr) const;
    operator bool() const { return IsValid(); }
  };

  // Compute pipeline

  /**
   * @brief
   * Graphics pipeline descriptor used for creating a ComputePipeline object
   */
  struct ComputePipelineDesc
  {
    const char* ComputeShaderPath{ nullptr };
    /** @brief EntryPoint == the name of the entrypoint of the shader (DX12 allows for custom entrypoints), usually "main" */
    const char* EntryPoint{ "main" };

    /**
     * @brief
     * where type == cs for compute shaders (type_ prefix gets added automatically in Device_DX12.cpp),
     * X == main version (e.g. 5 for hlsl shader language 5.1),
     * Y == subversion (e.g. 1 for 5.1).
     * Users should usually initialise this as pipelineDesc.ShaderVersion = "5_1" or something similar, unless they want to
     * use a shader type that has no support built into this library (so anything other than vertex, pixel or compute shaders)
     * See for example https://learn.microsoft.com/en-us/windows/win32/direct3dhlsl/specifying-compiler-targets for D3D12
     */
    const char* ShaderVersion{ nullptr };

    std::vector<PipelineResource> PipelineReadOnlyResources{};
    std::vector<PipelineResource> PipelineReadWriteResources{};
    std::vector<SamplerDesc> SamplerDescs{};

    /**
     * @brief
     * @param failureReason Filled in with the reason the object is invalid
     * @return true if ComputePipelineDesc is valid.
     * ComputePipelineDesc is valid when:
     * ComputeShaderPath != nullptr,
     * ShaderVersion != nullptr.
     */
    bool IsValid(std::string* failureReason = nullptr) const;
    operator bool() const { return IsValid(); }
  };

  /**
   * @brief
   * This is a compute pipeline object. Compute pipelines can be bound on a command buffer.
   */
  struct ComputePipeline
  {
    const ComputePipelineDesc Desc{};
    /** @brief A shared void pointer to API specific data */
    Ref<void> Data{ nullptr };

    ComputePipeline() = default;
    ComputePipeline(const ComputePipelineDesc& desc)
      : Desc(desc)
    {}
    ComputePipeline(const ComputePipeline& other)
      : Desc(other.Desc), Data(other.Data)
    {}
    ComputePipeline& operator=(const ComputePipeline& other)
    {
      this->~ComputePipeline();
      ComputePipeline* renderTarget = new (this) ComputePipeline(other);
      return *renderTarget;
    }

    /**
     * @brief
     * @param failureReason Filled in with the reason the object is invalid
     * @return true if ComputePipeline is valid.
     * ComputePipeline is valid when:
     * Desc is valid and Data != nullptr.
     */
    bool IsValid(std::string* failureReason = nullptr) const;
    operator bool() const { return IsValid(); }
  };
#pragma endregion
}
