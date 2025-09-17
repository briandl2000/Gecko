#pragma once

#include "Defines.h"
#include "Rendering/Frontend/ResourceManager/ResourceObjects.h"
#include "Rendering/Backend/Device.h"
#include "Core/Platform.h"
#include "Core/Event.h"

#include <unordered_map>

namespace Gecko
{

class ResourceManager
{
public:

  ResourceManager() = default;
  ~ResourceManager() {};

  void Init(Device* device);
  void Shutdown();

  // MeshHandle CreateMesh(VertexBufferDesc vertexDesc, IndexBufferDesc indexDesc, void* vertexData, void* indexData);
  BufferHandle CreateBuffer(VertexBufferDesc vertexDesc, void* vertexData);
  BufferHandle CreateBuffer(IndexBufferDesc indexDesc, void* indexData);
  BufferHandle CreateBuffer(BufferDesc bufferDesc, void* data);
  TextureHandle CreateTexture(TextureDesc textureDesc, void* imageData = nullptr, bool mipMap = false);
  RenderTargetHandle CreateRenderTarget(RenderTargetDesc renderTargetDesc);
  GraphicsPipelineHandle CreateGraphicsPipeline(GraphicsPipelineDesc graphicsPipelineDesc);
  ComputePipelineHandle CreateComputePipeline(ComputePipelineDesc computePipelineDesc);

  // Mesh& GetMesh(const MeshHandle& meshHandle);
  Buffer& GetBuffer(const BufferHandle& bufferHandle);
  Texture& GetTexture(const TextureHandle& textureHandle);
  RenderTarget& GetRenderTarget(const RenderTargetHandle& renderTargetHandle);
  GraphicsPipeline& GetGraphicsPipeline(const GraphicsPipelineHandle& graphicsPipelineHandle);
  ComputePipeline& GetComputePipeline(const ComputePipelineHandle& computePipelineHandle);

  TextureHandle GetMissingTextureHandle() { return m_MissingTextureHandle; }

  u32 GetCurrentBackBufferIndex() { return m_Device->GetCurrentBackBufferIndex(); }

  void UploadMaterial(Buffer& buffer, void* data, u32 size, u32 offset = 0);

private:
  void MipMapTexture(Texture texture);

private:

  Device* m_Device;

  TextureHandle m_MissingTextureHandle{ 0 };

  ComputePipelineHandle DownsamplePipelineHandle;

  u32 m_CurrentBufferIndex{ 0 };
  u32 m_CurrentTextureIndex{ 0 };
  u32 m_CurrentRenderTargetIndex{ 0 };
  u32 m_CurrentGraphicsPipelineIndex{ 0 };
  u32 m_CurrentComputePipelineIndex{ 0 };

  std::unordered_map<BufferHandle, Buffer> m_Buffers;
  std::unordered_map<TextureHandle, Texture> m_Textures;
  std::unordered_map<RenderTargetHandle, RenderTarget> m_RenderTargets;

  std::unordered_map<GraphicsPipelineHandle, GraphicsPipeline> m_GraphicsPipelines;
  std::unordered_map<ComputePipelineHandle, ComputePipeline> m_ComputePipelines;

};

}
