#if DIRECTX_12
#include "CommandList_DX12.h"

#include "Core/Logger.h"
#include "Rendering/DX12/Device_DX12.h"

#include "Objects_DX12.h"

#include "d3dx12.h"

namespace Gecko::DX12
{

	CommandList_DX12::~CommandList_DX12()
	{
	}

	bool CommandList_DX12::IsValid()
	{
		if (CommandBuffer == nullptr)
		{
			return false;
		}

		return CommandBuffer->CommandList != nullptr;
	}

	void CommandList_DX12::ClearRenderTarget(const RenderTarget& renderTarget)
	{
		ASSERT(renderTarget.IsValid(), "Render target is invalid!");
		
		RenderTarget_DX12* renderTargetDX12 = reinterpret_cast<RenderTarget_DX12*>(renderTarget.Data.get());
		TransitionRenderTarget(renderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		for (u32 i = 0; i < renderTarget.Desc.NumRenderTargets; i++)
		{
			CommandBuffer->CommandList->ClearRenderTargetView(
				renderTargetDX12->RenderTargetViews[i].CPU,
				renderTarget.Desc.RenderTargetClearValues[i].Values,
				1,
				&renderTargetDX12->Rect
			);
		}

		if (renderTarget.Desc.DepthStencilFormat != DataFormat::None)
		{
			CommandBuffer->CommandList->ClearDepthStencilView(
				renderTargetDX12->DepthStencilView.CPU,
				D3D12_CLEAR_FLAG_DEPTH,
				renderTarget.Desc.DepthTargetClearValue.Depth,
				0,
				1,
				&renderTargetDX12->Rect
			);
		}
	}

	void CommandList_DX12::CopyTextureToTexture(const Texture& src, const Texture& dst)
	{
		ASSERT(src.IsValid(), "Source texture is invalid!");
		ASSERT(dst.IsValid(), "Destination texture is invalid!");

		Texture_DX12* srcTexture_DX12 = reinterpret_cast<Texture_DX12*>(src.Data.get());
		Texture_DX12* dstTexture_DX12 = reinterpret_cast<Texture_DX12*>(dst.Data.get());

		Ref<Resource> srcTextureResource = srcTexture_DX12->TextureResource;
		Ref<Resource> dstTextureResource = dstTexture_DX12->TextureResource;

		TransitionResource(srcTextureResource, D3D12_RESOURCE_STATE_COPY_SOURCE, 1, 1);
		TransitionResource(dstTextureResource, D3D12_RESOURCE_STATE_COPY_DEST, 1, 1);

		CD3DX12_TEXTURE_COPY_LOCATION Src(srcTextureResource->ResourceDX12.Get());
		CD3DX12_TEXTURE_COPY_LOCATION Dst(dstTextureResource->ResourceDX12.Get());

		CommandBuffer->CommandList->CopyTextureRegion(
			&Dst,
			0,
			0,
			0,
			&Src,
			nullptr
		);
	}

	void CommandList_DX12::BindRenderTarget(const RenderTarget& renderTarget)
	{
		ASSERT(m_BoundPipelineType == PipelineType::Graphics, "Graphics pipeline needs to be bound to bind render target!");
		ASSERT(m_GraphicsPipeline.IsValid(), "Graphics pipeline is invalid!");
		ASSERT(renderTarget.IsValid(), "Render target is invalid!");

		RenderTarget_DX12* renderTargetDX12 = reinterpret_cast<RenderTarget_DX12*>(renderTarget.Data.get());

		TransitionRenderTarget(renderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_DEPTH_WRITE);

		CommandBuffer->CommandList->RSSetViewports(1, &renderTargetDX12->ViewPort);
		CommandBuffer->CommandList->RSSetScissorRects(1, &renderTargetDX12->Rect);

		D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetCpuHandles[8]{ 0 };
		for (u32 i = 0; i < renderTarget.Desc.NumRenderTargets; i++)
		{
			RenderTargetCpuHandles[i] = renderTargetDX12->RenderTargetViews[i].CPU;
		}

		CommandBuffer->CommandList->OMSetRenderTargets(
			renderTarget.Desc.NumRenderTargets,
			RenderTargetCpuHandles,
			false,
			renderTargetDX12->DepthStencilView.IsValid() ? &renderTargetDX12->DepthStencilView.CPU : nullptr);
	}

	void CommandList_DX12::BindVertexBuffer(const Buffer& vertexBuffer)
	{
		ASSERT(m_BoundPipelineType == PipelineType::Graphics, "Graphics pipeline needs to be bound for this!");
		ASSERT(m_GraphicsPipeline.IsValid(), "Graphics pipeline is invalid!");
		ASSERT(vertexBuffer.Desc.Type == BufferType::Vertex, "Buffer must be of type Vertex to be used as a vertex buffer!");
		ASSERT(vertexBuffer.IsValid(), "Vertex buffer is invalid!");
		Buffer_DX12* vertexBuffer_DX12 = reinterpret_cast<Buffer_DX12*>(vertexBuffer.Data.get());

		if (vertexBuffer.Desc.CanReadWrite)
		{
			TransitionResource(vertexBuffer_DX12->BufferResource, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, 0, 1);
		}
		CommandBuffer->CommandList->IASetVertexBuffers(0, 1, &vertexBuffer_DX12->VertexBufferView);
	}

	void CommandList_DX12::BindIndexBuffer(const Buffer& indexBuffer)
	{
		ASSERT(m_BoundPipelineType == PipelineType::Graphics, "Graphics pipeline needs to be bound for this!");
		ASSERT(m_GraphicsPipeline.IsValid(), "Graphics pipeline is invalid!");
		ASSERT(indexBuffer.Desc.Type == BufferType::Index, "Buffer must be of type Index to be used as an index buffer!");
		ASSERT(indexBuffer.IsValid(), "Index buffer is invalid!");
		Buffer_DX12* indexBuffer_DX12 = reinterpret_cast<Buffer_DX12*>(indexBuffer.Data.get());

		if (indexBuffer.Desc.CanReadWrite)
		{
			TransitionResource(indexBuffer_DX12->BufferResource, D3D12_RESOURCE_STATE_INDEX_BUFFER, 0, 1);
		}
		CommandBuffer->CommandList->IASetIndexBuffer(&indexBuffer_DX12->IndexBufferView);
	}

	void CommandList_DX12::BindConstantBuffer(u32 slot, const Buffer& buffer)
	{
		ASSERT(buffer.IsValid(), "Buffer is invalid!");
		ASSERT(buffer.Desc.Type == BufferType::Constant, "Buffer is invalid!");
		Buffer_DX12* constantBuffer_DX12 = reinterpret_cast<Buffer_DX12*>(buffer.Data.get());

		// TransitionResource(constantBuffer_DX12->BufferResource, D3D12_RESOURCE_STATE_COMMON, 0, 1);
		if (m_BoundPipelineType == PipelineType::Graphics)
		{
			ASSERT(m_GraphicsPipeline.IsValid(), "Graphics pipeline is Invalid!");
			GraphicsPipeline_DX12* graphicsPipeline_DX12 = reinterpret_cast<GraphicsPipeline_DX12*>(m_GraphicsPipeline.Data.get());
			ASSERT(slot < graphicsPipeline_DX12->ConstantBufferIndices.size(), "Slot is out of bounds of constant buffer indices!");

			u32 rootDescriptorTableSlot = graphicsPipeline_DX12->ConstantBufferIndices[slot];

			CommandBuffer->CommandList->SetGraphicsRootDescriptorTable(rootDescriptorTableSlot,
				constantBuffer_DX12->ConstantBufferView.GPU);
			return;
		}
		else if (m_BoundPipelineType == PipelineType::Compute)
		{
			ASSERT(m_ComputePipeline.IsValid(), "Compute pipeline is Invalid!");
			ComputePipeline_DX12* computePipeline_DX12 = reinterpret_cast<ComputePipeline_DX12*>(m_ComputePipeline.Data.get());
			ASSERT(slot < computePipeline_DX12->ConstantBufferIndices.size(), "Slot is out of bounds of constant buffer indices!");

			u32 rootDescriptorTableSlot = computePipeline_DX12->ConstantBufferIndices[slot];
			CommandBuffer->CommandList->SetComputeRootDescriptorTable(rootDescriptorTableSlot,
				constantBuffer_DX12->ConstantBufferView.GPU);
			return;
		}

		ASSERT(false, "No pipline bound!");
	}

	void CommandList_DX12::BindStructuredBuffer(u32 slot, const Buffer& buffer)
	{
		ASSERT(buffer.IsValid(), "Buffer is invalid!");
		//ASSERT_MSG(buffer.Desc.Type != BufferType::Constant, "ConstantBuffer Is not allow to be bound as StructuredBuffer!");
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle{ 0 };
		Ref<Resource> bufferResource = nullptr;

		Buffer_DX12* buffer_DX12 = reinterpret_cast<Buffer_DX12*>(buffer.Data.get());
		bufferResource = buffer_DX12->BufferResource;
		gpuHandle = buffer_DX12->ShaderResourceView.GPU;

		if (buffer.Desc.CanReadWrite)
		{
			TransitionResource(bufferResource, D3D12_RESOURCE_STATE_COMMON, 0, 1);
		}
		if (m_BoundPipelineType == PipelineType::Graphics)
		{
			ASSERT(m_GraphicsPipeline.IsValid(), "Graphics pipeline is Invalid!");
			GraphicsPipeline_DX12* graphicsPipeline_DX12 = reinterpret_cast<GraphicsPipeline_DX12*>(m_GraphicsPipeline.Data.get());
			ASSERT(slot < graphicsPipeline_DX12->TextureIndices.size(), "Slot is out of bounds of constant buffer indices!");
			TransitionResource(bufferResource, D3D12_RESOURCE_STATE_COMMON, 0, 1);

			u32 rootDescriptorTableSlot = graphicsPipeline_DX12->TextureIndices[slot];
			CommandBuffer->CommandList->SetGraphicsRootDescriptorTable(rootDescriptorTableSlot, gpuHandle);
			return;
		}
		else if (m_BoundPipelineType == PipelineType::Compute)
		{
			ASSERT(m_ComputePipeline.IsValid(), "Compute pipeline is Invalid!");
			ComputePipeline_DX12* computePipeline_DX12 = reinterpret_cast<ComputePipeline_DX12*>(m_ComputePipeline.Data.get());
			ASSERT(slot < computePipeline_DX12->TextureIndices.size(), "Slot is out of bounds of constant buffer indices!");
			TransitionResource(bufferResource, D3D12_RESOURCE_STATE_COMMON, 0, 1);

			u32 rootDescriptorTableSlot = computePipeline_DX12->TextureIndices[slot];
			CommandBuffer->CommandList->SetComputeRootDescriptorTable(rootDescriptorTableSlot, gpuHandle);
			return;
		}

		ASSERT(false, "No pipline bound!");
	}

	void CommandList_DX12::BindAsRWBuffer(u32 slot, const Buffer& buffer)
	{
		ASSERT(m_BoundPipelineType == PipelineType::Compute, "Compute pipeline must be bound to bind as read write texture!");
		ASSERT(m_ComputePipeline.IsValid(), "Compute pipeline is invalid!");
		ASSERT(buffer.IsValid(), "Buffer is invalid!");
		ASSERT(buffer.Desc.CanReadWrite, "Buffer must be created with CanReadWrite enabled to bind as RWBuffer!");

		Buffer_DX12* buffer_DX12 = reinterpret_cast<Buffer_DX12*>(buffer.Data.get());
		ComputePipeline_DX12* computePipeline_DX12 = reinterpret_cast<ComputePipeline_DX12*>(m_ComputePipeline.Data.get());
		ASSERT(slot < computePipeline_DX12->UAVIndices.size(), "Specified slot is out of bounds!");

		TransitionResource(buffer_DX12->BufferResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, 0, 1);

		u32	rootDescriptorTableSlot = computePipeline_DX12->UAVIndices[slot];
		CommandBuffer->CommandList->SetComputeRootDescriptorTable(rootDescriptorTableSlot, buffer_DX12->ReadWriteBufferView.GPU);
	}

	void CommandList_DX12::SetLocalData(u32 size, void* data)
	{
		ASSERT(size % 4 == 0, "The size of dynamic call data must be a multiple of 32 bits or 4 bytes");
		ASSERT(size != 0, "Data can't be 0");
		ASSERT(data != nullptr, "Data can't be nullptr");

		if (m_BoundPipelineType == PipelineType::Graphics)
		{
			ASSERT(m_GraphicsPipeline.IsValid(), "Graphics pipeline is Invalid!");
			GraphicsPipeline_DX12* graphicsPipeline_DX12 = reinterpret_cast<GraphicsPipeline_DX12*>(m_GraphicsPipeline.Data.get());

			u32 rootDescriptorTableSlot = graphicsPipeline_DX12->ConstantBufferIndices[graphicsPipeline_DX12->LocalDataLocation];
			CommandBuffer->CommandList->SetGraphicsRoot32BitConstants(rootDescriptorTableSlot, size / 4, data, 0);
			return;
		}
		else if (m_BoundPipelineType == PipelineType::Compute)
		{
			ASSERT(m_ComputePipeline.IsValid(), "Compute pipeline is Invalid!");
			ComputePipeline_DX12* computePipeline_DX12 = reinterpret_cast<ComputePipeline_DX12*>(m_ComputePipeline.Data.get());

			u32 rootDescriptorTableSlot = computePipeline_DX12->ConstantBufferIndices[computePipeline_DX12->LocalDataLocation];
			CommandBuffer->CommandList->SetComputeRoot32BitConstants(rootDescriptorTableSlot, size / 4, data, 0);
			return;
		}
		ASSERT_MSG(false, "No pipline bound!");
	}

	void CommandList_DX12::BindTexture(u32 slot, const Texture& texture)
	{
		ASSERT_MSG(texture.IsValid(), "Texture buffer is invalid!");
		Texture_DX12* texture_DX12 = reinterpret_cast<Texture_DX12*>(texture.Data.get());

		if (m_BoundPipelineType == PipelineType::Graphics)
		{
			ASSERT_MSG(m_GraphicsPipeline.IsValid(), "Graphics pipeline is invalid!");
			GraphicsPipeline_DX12* graphicsPipeline_DX12 = reinterpret_cast<GraphicsPipeline_DX12*>(m_GraphicsPipeline.Data.get());
			ASSERT_MSG(slot < graphicsPipeline_DX12->TextureIndices.size(), "Specified slot is out of bounds!");

			TransitionResource(texture_DX12->TextureResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				texture.Desc.NumMips, texture.Desc.NumArraySlices);

			u32 rootDescriptorTableSlot = graphicsPipeline_DX12->TextureIndices[slot];
			CommandBuffer->CommandList->SetGraphicsRootDescriptorTable(rootDescriptorTableSlot, texture_DX12->ShaderResourceView.GPU);
			return;
		}
		else if (m_BoundPipelineType == PipelineType::Compute)
		{
			ASSERT_MSG(m_ComputePipeline.IsValid(), "Compute pipeline is invalid!");
			ComputePipeline_DX12* computePipeline_DX12 = (ComputePipeline_DX12*)m_ComputePipeline.Data.get();
			ASSERT_MSG(slot < computePipeline_DX12->TextureIndices.size(), "Specified slot is out of bounds!");

			TransitionResource(texture_DX12->TextureResource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
				texture.Desc.NumMips, texture.Desc.NumArraySlices);

			u32 rootDescriptorTableSlot = computePipeline_DX12->TextureIndices[slot];
			CommandBuffer->CommandList->SetComputeRootDescriptorTable(rootDescriptorTableSlot, texture_DX12->ShaderResourceView.GPU);
			return;
		}

		ASSERT_MSG(false, "No pipline bound!");
	}

	void CommandList_DX12::BindTexture(u32 slot, const Texture& texture, u32 mipLevel)
	{
		ASSERT_MSG(texture.IsValid(), "Texture buffer is invalid!");
		ASSERT_MSG(mipLevel < texture.Desc.NumMips, "Mip level out of bounds of texture mips!");
		Texture_DX12* texture_DX12 = reinterpret_cast<Texture_DX12*>(texture.Data.get());

		if (m_BoundPipelineType == PipelineType::Graphics)
		{
			ASSERT_MSG(m_GraphicsPipeline.IsValid(), "Graphics pipeline is invalid!");
			GraphicsPipeline_DX12* graphicsPipeline_DX12 = reinterpret_cast<GraphicsPipeline_DX12*>(m_GraphicsPipeline.Data.get());
			ASSERT_MSG(slot < graphicsPipeline_DX12->TextureIndices.size(), "Specified slot is out of bounds!");

			TransitionSubResource(texture_DX12->TextureResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				texture.Desc.NumMips, texture.Desc.NumArraySlices, mipLevel);

			u32 rootDescriptorTableSlot = graphicsPipeline_DX12->TextureIndices[slot];
			CommandBuffer->CommandList->SetGraphicsRootDescriptorTable(rootDescriptorTableSlot,
				texture_DX12->MipShaderResourceViews[mipLevel].GPU);
			return;
		}
		else if (m_BoundPipelineType == PipelineType::Compute)
		{
			ASSERT_MSG(m_ComputePipeline.IsValid(), "Compute pipeline is invalid!");
			ComputePipeline_DX12* computePipeline_DX12 = reinterpret_cast<ComputePipeline_DX12*>(m_ComputePipeline.Data.get());
			ASSERT_MSG(slot < computePipeline_DX12->TextureIndices.size(), "Specified slot is out of bounds!");

			TransitionSubResource(texture_DX12->TextureResource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
				texture.Desc.NumMips, texture.Desc.NumArraySlices, mipLevel);

			u32 rootDescriptorTableSlot = computePipeline_DX12->TextureIndices[slot];
			CommandBuffer->CommandList->SetComputeRootDescriptorTable(rootDescriptorTableSlot,
				texture_DX12->MipShaderResourceViews[mipLevel].GPU);
			return;
		}

		ASSERT_MSG(false, "No pipline bound!");
	}

	void CommandList_DX12::BindAsRWTexture(u32 slot, const Texture& texture)
	{
		ASSERT_MSG(m_BoundPipelineType == PipelineType::Compute, "Compute pipeline must be bound to bind as read write texture!");
		ASSERT_MSG(m_ComputePipeline.IsValid(), "Compute pipeline is invalid!");
		ASSERT_MSG(texture.IsValid(), "Texture is invalid!");

		Texture_DX12* texture_DX12 = reinterpret_cast<Texture_DX12*>(texture.Data.get());
		ComputePipeline_DX12* computePipeline_DX12 = reinterpret_cast<ComputePipeline_DX12*>(m_ComputePipeline.Data.get());
		ASSERT_MSG(slot < computePipeline_DX12->UAVIndices.size(), "Specified slot is out of bounds!");

		TransitionResource(texture_DX12->TextureResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			texture.Desc.NumMips, texture.Desc.NumArraySlices);

		u32	rootDescriptorTableSlot = computePipeline_DX12->UAVIndices[slot];
		CommandBuffer->CommandList->SetComputeRootDescriptorTable(rootDescriptorTableSlot, texture_DX12->UnorderedAccessView.GPU);
	}

	void CommandList_DX12::BindAsRWTexture(u32 slot, const Texture& texture, u32 mipLevel)
	{
		ASSERT_MSG(m_BoundPipelineType == PipelineType::Compute, "Compute pipeline must be bound to bind as read write texture!");
		ASSERT_MSG(m_ComputePipeline.IsValid(), "Compute pipeline is invalid!");
		ASSERT_MSG(texture.IsValid(), "Texture is invalid!");
		ASSERT_MSG(mipLevel < texture.Desc.NumMips, "Specified mip is out of bounds of the texture!");

		Texture_DX12* texture_DX12 = reinterpret_cast<Texture_DX12*>(texture.Data.get());
		ComputePipeline_DX12* computePipeline_DX12 = reinterpret_cast<ComputePipeline_DX12*>(m_ComputePipeline.Data.get());
		ASSERT_MSG(slot < computePipeline_DX12->UAVIndices.size(), "Specified slot is out of bounds!");

		TransitionSubResource(texture_DX12->TextureResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
			texture.Desc.NumMips, texture.Desc.NumArraySlices, mipLevel);

		u32 rootDescriptorTableSlot = computePipeline_DX12->UAVIndices[slot];
		CommandBuffer->CommandList->SetComputeRootDescriptorTable(rootDescriptorTableSlot, texture_DX12->MipUnorderedAccessViews[mipLevel].GPU);
	}

	void CommandList_DX12::BindGraphicsPipeline(const GraphicsPipeline& graphicsPipeline)
	{
		ASSERT_MSG(graphicsPipeline.IsValid(), "Graphics pipeline is invalid!");

		GraphicsPipeline_DX12* graphicsPipeline_DX12 = reinterpret_cast<GraphicsPipeline_DX12*>(graphicsPipeline.Data.get());

		CommandBuffer->CommandList->SetPipelineState(graphicsPipeline_DX12->PipelineState.Get());
		CommandBuffer->CommandList->SetGraphicsRootSignature(graphicsPipeline_DX12->RootSignature.Get());

		CommandBuffer->CommandList->IASetPrimitiveTopology(PrimitiveTypeToD3D12PrimitiveTopology(graphicsPipeline.Desc.PrimitiveType));

		m_BoundPipelineType = PipelineType::Graphics;
		m_GraphicsPipeline = graphicsPipeline;
	}

	void CommandList_DX12::BindComputePipeline(const ComputePipeline& computePipeline)
	{
		ASSERT_MSG(computePipeline.IsValid(), "Compute pipeline is invalid!");

		ComputePipeline_DX12* computePipeline_DX12 = reinterpret_cast<ComputePipeline_DX12*>(computePipeline.Data.get());

		CommandBuffer->CommandList->SetPipelineState(computePipeline_DX12->PipelineState.Get());
		CommandBuffer->CommandList->SetComputeRootSignature(computePipeline_DX12->RootSignature.Get());

		m_BoundPipelineType = PipelineType::Compute;
		m_ComputePipeline = computePipeline;
	}

	void CommandList_DX12::Draw(u32 numIndices)
	{
		ASSERT_MSG(m_BoundPipelineType == PipelineType::Graphics, "Graphics pipeline needs to be bound to draw!");
		ASSERT_MSG(m_GraphicsPipeline.IsValid(), "Graphics pipeline is invalid!");
		ASSERT_MSG(numIndices != 0, "Number of indices cannot be 0!");
		CommandBuffer->CommandList->DrawIndexedInstanced(numIndices, 1, 0, 0, 0);
	}

	void CommandList_DX12::DrawAuto(u32 numVertices)
	{
		ASSERT_MSG(m_BoundPipelineType == PipelineType::Graphics, "Graphics pipeline needs to be bound to draw!");
		ASSERT_MSG(m_GraphicsPipeline.IsValid(), "Graphics pipeline is invalid!");
		ASSERT_MSG(numVertices != 0, "Number of vertices cannot be 0!");
		CommandBuffer->CommandList->DrawInstanced(numVertices, 1, 0, 0);
	}

	void CommandList_DX12::Dispatch(u32 xThreads, u32 yThreads, u32 zThreads)
	{
		ASSERT(m_BoundPipelineType == PipelineType::Compute, "Compute pipeline needs to be bound to draw!");
		ASSERT(m_ComputePipeline.IsValid(), "Compute pipeline is invalid!");
		ASSERT(xThreads*yThreads*zThreads != 0, "All thread dimensions need to be at least 1!");
		CommandBuffer->CommandList->Dispatch(xThreads, yThreads, zThreads);
	}

	void CommandList_DX12::TransitionSubResource(const Ref<Resource>& resource, D3D12_RESOURCE_STATES transtion, u32 numMips, u32 numArraySlices, u32 subResourceIndex)
	{
		std::vector<CD3DX12_RESOURCE_BARRIER> barriers{ };

		GetTextureMipTransitionBarriers(&barriers, resource, transtion, numMips, numArraySlices, subResourceIndex);

		if (barriers.size() == 0) return;

		CommandBuffer->CommandList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
		resource->SubResourceStates[subResourceIndex] = transtion;
	}

	void CommandList_DX12::TransitionResource(const Ref<Resource>& resource, D3D12_RESOURCE_STATES transtion, u32 numMips, u32 numArraySlices)
	{
		std::vector<CD3DX12_RESOURCE_BARRIER> barriers{ };

		GetTextureTransitionBarriers(&barriers, resource, transtion, numMips, numArraySlices);

		if (barriers.size() == 0) return;

		CommandBuffer->CommandList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
		resource->CurrentState = transtion;
		for (u32 i = 0; i < resource->SubResourceStates.size(); i++)
		{
			resource->SubResourceStates[i] = transtion;
		}
	}

	void CommandList_DX12::TransitionRenderTarget(const RenderTarget& renderTarget, D3D12_RESOURCE_STATES newRenderTargetState,
		D3D12_RESOURCE_STATES newDepthStencilState)
	{
		std::vector<CD3DX12_RESOURCE_BARRIER> barriers{ };

		ASSERT(renderTarget.IsValid(), "Invalid render target in transition render target!");

		for (u32 i = 0; i < renderTarget.Desc.NumRenderTargets; i++)
		{
			Texture_DX12* texture = reinterpret_cast<Texture_DX12*>(renderTarget.RenderTextures[i].Data.get());
			GetTextureTransitionBarriers(&barriers, texture->TextureResource, newRenderTargetState,
				renderTarget.RenderTextures[i].Desc.NumMips, renderTarget.RenderTextures[i].Desc.NumArraySlices);

			texture->TextureResource->CurrentState = newRenderTargetState;
			for (u32 j = 0; j < static_cast<u32>(texture->TextureResource->SubResourceStates.size()); j++)
			{
				texture->TextureResource->SubResourceStates[j] = newRenderTargetState;
			}
		}
		if (renderTarget.DepthTexture.IsValid())
		{
			Texture_DX12* depthTexture = reinterpret_cast<Texture_DX12*>(renderTarget.DepthTexture.Data.get());
			GetTextureTransitionBarriers(&barriers, depthTexture->TextureResource, newDepthStencilState,
				renderTarget.DepthTexture.Desc.NumMips, renderTarget.DepthTexture.Desc.NumArraySlices);

			depthTexture->TextureResource->CurrentState = newDepthStencilState;
			for (u32 i = 0; i < static_cast<u32>(depthTexture->TextureResource->SubResourceStates.size()); i++)
			{
				depthTexture->TextureResource->SubResourceStates[i] = newDepthStencilState;
			}
		}

		if (barriers.size() == 0) return;

		CommandBuffer->CommandList->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());
	}

	void CommandList_DX12::GetTextureMipTransitionBarriers(std::vector<CD3DX12_RESOURCE_BARRIER>* barriers, const Ref<Resource>& resource,
		D3D12_RESOURCE_STATES transtion, u32 numMips, u32 numArraySlices, u32 subResourceIndex)
	{
		if (resource->SubResourceStates[subResourceIndex] == transtion)
			return;

		for (u32 i = 0; i < numArraySlices; i++)
		{
			barriers->push_back(CD3DX12_RESOURCE_BARRIER::Transition(
				resource->ResourceDX12.Get(),
				resource->SubResourceStates[subResourceIndex],
				transtion,
				subResourceIndex + i * numMips
			));
		}
	}

	void CommandList_DX12::GetTextureTransitionBarriers(std::vector<CD3DX12_RESOURCE_BARRIER>* barriers, const Ref<Resource>& resource,
		D3D12_RESOURCE_STATES transtion, u32 numMips, u32 numArraySlices)
	{
		if (resource->CurrentState == transtion)
		{
			for (u32 i = 0; i < resource->SubResourceStates.size(); i++)
			{
				GetTextureMipTransitionBarriers(barriers, resource, transtion, numMips, numArraySlices, i);
			}
			return;
		}

		for (u32 i = 0; i < resource->SubResourceStates.size(); i++)
		{
			if (resource->SubResourceStates[i] != resource->CurrentState)
			{
				GetTextureMipTransitionBarriers(barriers, resource, resource->CurrentState, numMips, numArraySlices, i);
			}
		}

		barriers->push_back(CD3DX12_RESOURCE_BARRIER::Transition(
			resource->ResourceDX12.Get(),
			resource->CurrentState,
			transtion
		));
	}
}


#endif