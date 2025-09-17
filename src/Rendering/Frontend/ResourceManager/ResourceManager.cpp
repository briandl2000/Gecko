#include "Rendering/Frontend/ResourceManager/ResourceManager.h"

#include "Core/Asserts.h"
#include "Rendering/Backend/CommandList.h"

#if defined(WIN32)
#pragma warning( push )
#pragma warning(disable : 4244)
#endif

#define STBI_MALLOC(sz) Gecko::Platform::CustomAllocate(sz)
#define STBI_REALLOC(p,newsz) Gecko::Platform::CustomRealloc(p,newsz)
#define STBI_FREE(p) Gecko::Platform::CustomFree(p)

#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#if defined(WIN32)
#pragma warning( pop )
#endif

namespace Gecko
{

  struct MipGenerationData
  {
    u32 Mip;
    u32 Srgb;
    u32 Width;
    u32 Height;
  };

  void ResourceManager::Init(Device* device)
  {
    m_Device = device;
    m_CurrentBufferIndex = 0;
    m_CurrentTextureIndex = 0;
    m_CurrentRenderTargetIndex = 0;

    // MipMap Compute Pipeline
    {
      ComputePipelineDesc computePipelineDesc;
      computePipelineDesc.ComputeShaderPath = "Shaders/DownSample.gsh";
      computePipelineDesc.ShaderVersion = "5_1";
      computePipelineDesc.PipelineReadOnlyResources = 
      {
        PipelineResource::LocalData(ShaderType::Compute, 0, sizeof(MipGenerationData)), 
        PipelineResource::Texture(ShaderType::Compute, 0)
      };
      computePipelineDesc.PipelineReadWriteResources = 
      {
        PipelineResource::Texture(ShaderType::Compute, 0)
      };
      computePipelineDesc.SamplerDescs = {
        {ShaderType::Pixel, SamplerFilter::Linear}
      };

      DownsamplePipelineHandle = CreateComputePipeline(computePipelineDesc);
    }

    // Create missing Texture
    {
      TextureDesc textureDesc;
      textureDesc.Width = 512;
      textureDesc.Height = 512;
      textureDesc.Type = TextureType::Tex2D;
      textureDesc.Format = DataFormat::R8G8B8A8_SRGB;
      textureDesc.NumMips = CalculateNumberOfMips(textureDesc.Width, textureDesc.Height);
      textureDesc.NumArraySlices = 1;

      std::vector<u32> emptyTexData;
      emptyTexData.resize(textureDesc.Width * textureDesc.Height);

      u32 ColorA = 0xFF8a817d;
      u32 ColorB = 0xFFf54e0c;

      for (u32 x = 0; x < textureDesc.Width; x++)
      {
        for (u32 y = 0; y < textureDesc.Height; y++)
        {
          u32 index = x + y * textureDesc.Width;
          if (((x / 8) + (y / 8)) % 2 == 0)
          {
            emptyTexData[index] = ColorA;
          }
          else
          {
            emptyTexData[index] = ColorB;
          }
        }
      }

      m_MissingTextureHandle = CreateTexture(textureDesc, emptyTexData.data(), true);
    }
  }

  void ResourceManager::Shutdown()
  {
    m_CurrentBufferIndex = 0;
    m_CurrentTextureIndex = 0;
    m_CurrentRenderTargetIndex = 0;
    m_CurrentGraphicsPipelineIndex = 0;
    m_CurrentComputePipelineIndex = 0;

    m_Textures.clear();
    m_Buffers.clear();
    m_RenderTargets.clear();
    m_GraphicsPipelines.clear();
    m_ComputePipelines.clear();
  }

  BufferHandle ResourceManager::CreateBuffer(VertexBufferDesc vertexDesc, void* vertexData)
  {
    BufferHandle handle = m_CurrentBufferIndex;

    Buffer buffer;

    buffer = m_Device->CreateVertexBuffer(vertexDesc);

    m_Device->UploadBufferData(buffer, vertexData, buffer.Desc.Stride * buffer.Desc.NumElements);

    m_Buffers[handle] = buffer;

    m_CurrentBufferIndex++;
    return handle;
  }

  BufferHandle ResourceManager::CreateBuffer(IndexBufferDesc indexDesc, void* indexData)
  {
    BufferHandle handle = m_CurrentBufferIndex;

    Buffer buffer;

    buffer = m_Device->CreateIndexBuffer(indexDesc);

    m_Device->UploadBufferData(buffer, indexData, buffer.Desc.Stride * buffer.Desc.NumElements);

    m_Buffers[handle] = buffer;

    m_CurrentBufferIndex++;
    return handle;
  }

  TextureHandle ResourceManager::CreateTexture(TextureDesc textureDesc, void* imageData, bool mipMap)
  {
    TextureHandle handle = m_CurrentTextureIndex;
  
    Texture outTex = m_Device->CreateTexture(textureDesc);
    m_Textures[handle] = outTex;

    if (imageData != nullptr)
    {
      m_Device->UploadTextureData(m_Textures[handle], imageData, 0, 0);
    }
    if (mipMap)
    {
      MipMapTexture(m_Textures[handle]);
    }

    m_CurrentTextureIndex++;
    return handle;
  }

  RenderTargetHandle ResourceManager::CreateRenderTarget(RenderTargetDesc renderTargetDesc)
  {
    RenderTargetHandle handle = m_CurrentRenderTargetIndex;

    RenderTarget outRenderTarget = m_Device->CreateRenderTarget(renderTargetDesc);
    m_RenderTargets[handle] = outRenderTarget;

    m_CurrentRenderTargetIndex++;
    return handle;
  }

  GraphicsPipelineHandle ResourceManager::CreateGraphicsPipeline(GraphicsPipelineDesc graphicsPipelineDesc)
  {
    GraphicsPipelineHandle handle = m_CurrentGraphicsPipelineIndex;

    GraphicsPipeline outGraphicsPipeline = m_Device->CreateGraphicsPipeline(graphicsPipelineDesc);
    m_GraphicsPipelines[handle] = outGraphicsPipeline;

    m_CurrentGraphicsPipelineIndex++;
    return handle;
  }

  ComputePipelineHandle ResourceManager::CreateComputePipeline(ComputePipelineDesc computePipelineDesc)
  {
    ComputePipelineHandle handle = m_CurrentComputePipelineIndex;

    ComputePipeline outComputePipeline = m_Device->CreateComputePipeline(computePipelineDesc);
    m_ComputePipelines[handle] = outComputePipeline;

    m_CurrentComputePipelineIndex++;
    return handle;
  }

  Buffer& ResourceManager::GetBuffer(const BufferHandle& bufferHandle)
  {
    // TODO: raise error when buffer not found
    return m_Buffers[bufferHandle];
  }

  Texture& ResourceManager::GetTexture(const TextureHandle& textureHandle)
  {
    if (m_Textures.find(textureHandle) == m_Textures.end())
      return m_Textures[m_MissingTextureHandle];

    return m_Textures[textureHandle];
  }

  RenderTarget& ResourceManager::GetRenderTarget(const RenderTargetHandle& renderTargetHandle)
  {
    if (m_RenderTargets.find(renderTargetHandle) == m_RenderTargets.end())
    {
      ASSERT(false, "Could not find specified RenderTarget!");
    }

    return  m_RenderTargets[renderTargetHandle];
  }

  GraphicsPipeline& ResourceManager::GetGraphicsPipeline(const GraphicsPipelineHandle& graphicsPipelineHandle)
  {
    if (m_GraphicsPipelines.find(graphicsPipelineHandle) == m_GraphicsPipelines.end())
    {
      ASSERT(false, "Could not find specified GraphicsPipeline!");
    }

    return m_GraphicsPipelines[graphicsPipelineHandle];
  }

  ComputePipeline& ResourceManager::GetComputePipeline(const ComputePipelineHandle& computePipelineHandle)
  {
    if (m_ComputePipelines.find(computePipelineHandle) == m_ComputePipelines.end())
    {
      ASSERT(false, "Could not find specified ComputePipeline!");
    }

    return m_ComputePipelines[computePipelineHandle];
  }

  void ResourceManager::UploadMaterial(Buffer& buffer, void* data, u32 size, u32 offset)
  {
    m_Device->UploadBufferData(buffer, data, size, offset);
  }

  void ResourceManager::MipMapTexture(Texture texture)
  {
    MipGenerationData mipGenerationData;

    mipGenerationData.Mip = 0;
    mipGenerationData.Srgb = texture.Desc.Format == DataFormat::R8G8B8A8_SRGB ? 1 : 0;
    mipGenerationData.Width = texture.Desc.Width;
    mipGenerationData.Height = texture.Desc.Height;

    while (mipGenerationData.Mip < texture.Desc.NumMips - 1)
    {

      Ref<CommandList> commandList = m_Device->CreateComputeCommandList();
      commandList->BindComputePipeline(GetComputePipeline(DownsamplePipelineHandle));
      commandList->SetLocalData(sizeof(MipGenerationData), &mipGenerationData);

      commandList->BindTexture(0, texture, mipGenerationData.Mip);

      commandList->BindAsRWTexture(0, texture, mipGenerationData.Mip + 1);

      commandList->Dispatch(
        std::max(1u, mipGenerationData.Width),
        std::max(1u, mipGenerationData.Height),
        std::max(1u, texture.Desc.NumArraySlices)
      );

      mipGenerationData.Mip += 1;
      mipGenerationData.Width = mipGenerationData.Width >> 1;
      mipGenerationData.Height = mipGenerationData.Height >> 1;
      m_Device->ExecuteComputeCommandList(commandList);
    }

  }

}
