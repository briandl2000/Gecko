#if WIN32
#include "CommandList_DX12.h"

#include "Logging/Logger.h"
#include "Rendering/DX12/Device_DX12.h"

#include "Objects_DX12.h"

#include "d3dx12.h"

namespace Gecko { namespace DX12 { 

	CommandList_DX12::~CommandList_DX12()
	{}

	bool CommandList_DX12::IsValid()
	{
		return CommandBuffer->CommandList.Get() != nullptr;
	}
	
	void CommandList_DX12::ClearRenderTarget(RenderTarget renderTarget)
	{
		RenderTarget_DX12* renderTargetDX12 = (RenderTarget_DX12*)renderTarget.Data.get();
		TransitionRenderTarget(renderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_DEPTH_WRITE);
		for (u32 i = 0; i < renderTarget.Desc.NumRenderTargets; i++)
		{
			CommandBuffer->CommandList->ClearRenderTargetView(
				renderTargetDX12->rtvs[i].CPU,
				renderTarget.Desc.RenderTargetClearValues[i].Values,
				1,
				&renderTargetDX12->rect
			);
		}

		if (renderTarget.Desc.DepthStencilFormat != Format::None)
		{
			CommandBuffer->CommandList->ClearDepthStencilView(
				renderTargetDX12->dsv.CPU,
				D3D12_CLEAR_FLAG_DEPTH,
				renderTarget.Desc.DepthTargetClearValue.Depth,
				0,
				1,
				&renderTargetDX12->rect
			);
		}
	}

	void CommandList_DX12::CopyToRenderTarget(RenderTarget src, RenderTargetType srcType, RenderTarget dst, RenderTargetType dstType)
	{
		RenderTarget_DX12* srcRenderTarget_DX12 = (RenderTarget_DX12*)src.Data.get();
		RenderTarget_DX12* dstRenderTarget_DX12 = (RenderTarget_DX12*)dst.Data.get();
		
		Format srcFormat = Format::None;
		Format dstFormat = Format::None;

		Ref<Resource> srcRenderTargetResource;
		Ref<Resource> dstRenderTargetResource;

		if (srcType == RenderTargetType::TargetDepth)
		{
			srcFormat = src.Desc.DepthStencilFormat;
			srcRenderTargetResource = srcRenderTarget_DX12->DepthBufferResource;
		}
		else
		{
			u32 resourceIndex = GetRenderTargetResourceIndex(srcType);
			srcFormat = src.Desc.RenderTargetFormats[resourceIndex];
			srcRenderTargetResource = srcRenderTarget_DX12->RenderTargetResources[resourceIndex];
		}

		if (dstType == RenderTargetType::TargetDepth)
		{
			dstFormat = dst.Desc.DepthStencilFormat;
			dstRenderTargetResource = dstRenderTarget_DX12->DepthBufferResource;
		}
		else
		{
			u32 resourceIndex = GetRenderTargetResourceIndex(dstType);
			dstFormat = dst.Desc.RenderTargetFormats[resourceIndex];
			dstRenderTargetResource = dstRenderTarget_DX12->RenderTargetResources[resourceIndex];
		}

		TransitionResource(dstRenderTargetResource, D3D12_RESOURCE_STATE_COPY_DEST, 1, 1);
		TransitionResource(srcRenderTargetResource, D3D12_RESOURCE_STATE_COPY_SOURCE, 1, 1);

		CD3DX12_TEXTURE_COPY_LOCATION Dst(dstRenderTargetResource->Resource.Get());
		CD3DX12_TEXTURE_COPY_LOCATION Src(srcRenderTargetResource->Resource.Get());

		CommandBuffer->CommandList->CopyTextureRegion(
			&Dst,
			0, 
			0, 
			0, 
			&Src,
			nullptr
		);
	}
	
	void CommandList_DX12::CopyFromRenderTarget(RenderTarget src, RenderTargetType srcType, Texture dst)
	{
		RenderTarget_DX12* srcRenderTarget_DX12 = (RenderTarget_DX12*)src.Data.get();
		Texture_DX12* dstTexture_DX12 = (Texture_DX12*)dst.Data.get();
		
		Format srcFormat = Format::None;
		Format dstFormat = Format::None;

		Ref<Resource> srcRenderTargetResource;
		Ref<Resource> dstRenderTargetResource;

		if (srcType == RenderTargetType::TargetDepth)
		{
			srcFormat = src.Desc.DepthStencilFormat;
			srcRenderTargetResource = srcRenderTarget_DX12->DepthBufferResource;
		}
		else
		{
			u32 resourceIndex = GetRenderTargetResourceIndex(srcType);
			srcFormat = src.Desc.RenderTargetFormats[resourceIndex];
			srcRenderTargetResource = srcRenderTarget_DX12->RenderTargetResources[resourceIndex];
		}

		dstFormat = dst.Desc.Format;
		dstRenderTargetResource = dstTexture_DX12->TextureResource;
		
		TransitionResource(dstRenderTargetResource, D3D12_RESOURCE_STATE_COPY_DEST, 1, 1);
		TransitionResource(srcRenderTargetResource, D3D12_RESOURCE_STATE_COPY_SOURCE, 1, 1);

		CD3DX12_TEXTURE_COPY_LOCATION Dst(dstRenderTargetResource->Resource.Get());
		CD3DX12_TEXTURE_COPY_LOCATION Src(srcRenderTargetResource->Resource.Get());

		CommandBuffer->CommandList->CopyTextureRegion(
			&Dst,
			0, 
			0, 
			0, 
			&Src,
			nullptr
		);
	}

	void CommandList_DX12::CopyFromTexture(Texture src, RenderTarget dst, RenderTargetType dstType)
	{

		Texture_DX12* srcTexture_DX12 = (Texture_DX12*)src.Data.get();
		RenderTarget_DX12* dstRenderTarget_DX12 = (RenderTarget_DX12*)dst.Data.get();

		Format srcFormat = Format::None;
		Format dstFormat = Format::None;

		Ref<Resource> srcRenderTargetResource;
		Ref<Resource> dstRenderTargetResource;

		srcFormat = src.Desc.Format;
		srcRenderTargetResource = srcTexture_DX12->TextureResource;

		if (dstType == RenderTargetType::TargetDepth)
		{
			dstFormat = dst.Desc.DepthStencilFormat;
			dstRenderTargetResource = dstRenderTarget_DX12->DepthBufferResource;
		}
		else
		{
			u32 resourceIndex = GetRenderTargetResourceIndex(dstType);
			dstFormat = dst.Desc.RenderTargetFormats[resourceIndex];
			dstRenderTargetResource = dstRenderTarget_DX12->RenderTargetResources[resourceIndex];
		}


		TransitionResource(dstRenderTargetResource, D3D12_RESOURCE_STATE_COPY_DEST, 1, 1);
		TransitionResource(srcRenderTargetResource, D3D12_RESOURCE_STATE_COPY_SOURCE, 1, 1);

		CD3DX12_TEXTURE_COPY_LOCATION Dst(dstRenderTargetResource->Resource.Get());
		CD3DX12_TEXTURE_COPY_LOCATION Src(srcRenderTargetResource->Resource.Get());

		CommandBuffer->CommandList->CopyTextureRegion(
			&Dst,
			0,
			0,
			0,
			&Src,
			nullptr
		);
	}

	void CommandList_DX12::TransitionResource(Ref<Resource> resource, D3D12_RESOURCE_STATES transtion, u32 numMips, u32 numArraySlices)
	{
		
		if (resource->CurrentState == transtion)
		{
			for (u32 i = 0; i < resource->subResourceStates.size(); i++)
			{
				TransitionSubResource(resource, transtion, numMips, numArraySlices, i);
			}
			return;
		}

		for (u32 i = 0; i < resource->subResourceStates.size(); i++)
		{
			if(resource->subResourceStates[i] != resource->CurrentState)
				TransitionSubResource(resource, resource->CurrentState, numMips, numArraySlices, i);
		}

		CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
			resource->Resource.Get(),
			resource->CurrentState,
			transtion
		);

		CommandBuffer->CommandList->ResourceBarrier(1, &barrier);
		resource->CurrentState = transtion;
		
		for (u32 i = 0; i < resource->subResourceStates.size(); i++)
		{
			resource->subResourceStates[i] = transtion;
		}
	}

	void CommandList_DX12::TransitionSubResource(
		Ref<Resource> resource, 
		D3D12_RESOURCE_STATES transtion, 
		u32 numMips,
		u32 numArraySlices,
		u32 subResourceIndex)
	{
		if (resource->subResourceStates[subResourceIndex] == transtion)
			return;

		for (u32 i = 0; i < numArraySlices; i++)
		{
			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				resource->Resource.Get(),
				resource->subResourceStates[subResourceIndex],
				transtion,
				subResourceIndex + i * numMips
			);
			CommandBuffer->CommandList->ResourceBarrier(1, &barrier);
		}
		resource->subResourceStates[subResourceIndex] = transtion;
	}

	void CommandList_DX12::BindRenderTarget(RenderTarget renderTarget)
	{
		//ASSERT_MSG(CurrentlyGraphicsBoundPipeline != nullptr, "Graphics pipeline needs to be bound for this!");

		RenderTarget_DX12* renderTargetDX12 = (RenderTarget_DX12*)renderTarget.Data.get();

		TransitionRenderTarget(renderTarget, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_DEPTH_WRITE);

		CommandBuffer->CommandList->RSSetViewports(1, &renderTargetDX12->ViewPort);
		CommandBuffer->CommandList->RSSetScissorRects(1, &renderTargetDX12->rect);

		D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetCpuHandles[8]{0};
		for (u32 i = 0; i < renderTarget.Desc.NumRenderTargets; i++)
		{
			RenderTargetCpuHandles[i] = renderTargetDX12->rtvs[i].CPU;
		}

		CommandBuffer->CommandList->OMSetRenderTargets(
			renderTarget.Desc.NumRenderTargets, 
			RenderTargetCpuHandles,
			renderTargetDX12->DepthBufferResource != nullptr, 
			renderTargetDX12->DepthBufferResource != nullptr ? &renderTargetDX12->dsv.CPU : nullptr);
	}

	void CommandList_DX12::BindVertexBuffer(VertexBuffer vertexBuffer)
	{
		ASSERT_MSG(m_BoundPipelineType == PipelineType::Graphics, "Graphics pipeline needs to be bound for this!");
		VertexBuffer_DX12* vertexBuffer_DX12 = (VertexBuffer_DX12*)vertexBuffer.Data.get();

		CommandBuffer->CommandList->IASetVertexBuffers(0, 1, &vertexBuffer_DX12->VertexBufferView);
	}
	
	void CommandList_DX12::BindIndexBuffer(IndexBuffer indexBuffer)
	{
		ASSERT_MSG(m_BoundPipelineType == PipelineType::Graphics, "Graphics pipeline needs to be bound for this!");
		IndexBuffer_DX12* indexBuffer_DX12 = (IndexBuffer_DX12*)indexBuffer.Data.get();

		CommandBuffer->CommandList->IASetIndexBuffer(&indexBuffer_DX12->IndexBufferView);

	}
	
	void CommandList_DX12::BindTexture(u32 slot, Texture texture)
	{
		Texture_DX12* texture_DX12 = (Texture_DX12*)texture.Data.get();

		u32 rootDescriptorTableSlot = 0;
		if (m_BoundPipelineType == PipelineType::Graphics)
		{
			TransitionResource(texture_DX12->TextureResource, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, texture.Desc.NumMips, texture.Desc.NumArraySlices);
			GraphicsPipeline_DX12* graphicsPipeline_DX12 = (GraphicsPipeline_DX12*)m_GraphicsPipeline.Data.get();
			rootDescriptorTableSlot = graphicsPipeline_DX12->TextureIndices[slot];
			CommandBuffer->CommandList->SetGraphicsRootDescriptorTable(rootDescriptorTableSlot, texture_DX12->srv.GPU);
		}
		else if(m_BoundPipelineType == PipelineType::Compute)
		{
			TransitionResource(texture_DX12->TextureResource, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, texture.Desc.NumMips, texture.Desc.NumArraySlices);
			ComputePipeline_DX12* computePipeline_DX12 = (ComputePipeline_DX12*)m_ComputePipeline.Data.get();
			rootDescriptorTableSlot = computePipeline_DX12->TextureIndices[slot];
			CommandBuffer->CommandList->SetComputeRootDescriptorTable(rootDescriptorTableSlot, texture_DX12->srv.GPU);
		}
		else
		{
			ASSERT_MSG(false, "No pipline bound!");
			return;
		}

	}

	void CommandList_DX12::BindTexture(u32 slot, Texture texture, u32 mipLevel)
	{
		Texture_DX12* texture_DX12 = (Texture_DX12*)texture.Data.get();
		
		u32 rootDescriptorTableSlot = 0;
		if (m_BoundPipelineType == PipelineType::Graphics)
		{
			TransitionSubResource(
				texture_DX12->TextureResource, 
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
				texture.Desc.NumMips,
				texture.Desc.NumArraySlices, 
				mipLevel
			);
			GraphicsPipeline_DX12* graphicsPipeline_DX12 = (GraphicsPipeline_DX12*)m_GraphicsPipeline.Data.get();

			rootDescriptorTableSlot = graphicsPipeline_DX12->TextureIndices[slot];

			CommandBuffer->CommandList->SetGraphicsRootDescriptorTable(rootDescriptorTableSlot, texture_DX12->mipSrvs[mipLevel].GPU);
		}
		else if (m_BoundPipelineType == PipelineType::Compute)
		{
			TransitionSubResource(
				texture_DX12->TextureResource, 
				D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, 
				texture.Desc.NumMips,
				texture.Desc.NumArraySlices, 
				mipLevel
			);
			ComputePipeline_DX12* computePipeline_DX12 = (ComputePipeline_DX12*)m_ComputePipeline.Data.get();
			rootDescriptorTableSlot = computePipeline_DX12->TextureIndices[slot];
			CommandBuffer->CommandList->SetComputeRootDescriptorTable(rootDescriptorTableSlot, texture_DX12->mipSrvs[mipLevel].GPU);
		}
		else
		{
			ASSERT_MSG(false, "No pipline bound!");
			return;
		}

	}

	void CommandList_DX12::BindTexture(u32 slot, RenderTarget renderTarget, RenderTargetType type)
	{
		RenderTarget_DX12* renderTarget_DX12 = (RenderTarget_DX12*)renderTarget.Data.get();

		DescriptorHandle* descriptorHandle;
		switch (type)
		{
		case RenderTargetType::Target0: descriptorHandle = &renderTarget_DX12->renderTargetSrvs[0]; break;
		case RenderTargetType::Target1: descriptorHandle = &renderTarget_DX12->renderTargetSrvs[1]; break;
		case RenderTargetType::Target2: descriptorHandle = &renderTarget_DX12->renderTargetSrvs[2]; break;
		case RenderTargetType::Target3: descriptorHandle = &renderTarget_DX12->renderTargetSrvs[3]; break;
		case RenderTargetType::Target4: descriptorHandle = &renderTarget_DX12->renderTargetSrvs[4]; break;
		case RenderTargetType::Target5: descriptorHandle = &renderTarget_DX12->renderTargetSrvs[5]; break;
		case RenderTargetType::Target6: descriptorHandle = &renderTarget_DX12->renderTargetSrvs[6]; break;
		case RenderTargetType::Target7: descriptorHandle = &renderTarget_DX12->renderTargetSrvs[7]; break;
		case RenderTargetType::Target8: descriptorHandle = &renderTarget_DX12->renderTargetSrvs[8]; break;
		case RenderTargetType::TargetDepth: descriptorHandle = &renderTarget_DX12->depthStencilSrv; break;
		default:
			ASSERT_MSG(false, "Unkown renderTargetType");
			return;
			break;
		}
		if (m_BoundPipelineType == PipelineType::Graphics)
		{
			GraphicsPipeline_DX12* graphicsPipeline_DX12 = (GraphicsPipeline_DX12*)m_GraphicsPipeline.Data.get();
			u32 rootDescriptorTableSlot = graphicsPipeline_DX12->TextureIndices[slot];
			TransitionRenderTarget(renderTarget, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_DEPTH_READ);
			CommandBuffer->CommandList->SetGraphicsRootDescriptorTable(rootDescriptorTableSlot, descriptorHandle->GPU);
		}
		else if (m_BoundPipelineType == PipelineType::Compute)
		{
			ComputePipeline_DX12* computePipeline_DX12 = (ComputePipeline_DX12*)m_ComputePipeline.Data.get();
			u32 rootDescriptorTableSlot = computePipeline_DX12->TextureIndices[slot];
			TransitionRenderTarget(renderTarget, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
			CommandBuffer->CommandList->SetComputeRootDescriptorTable(rootDescriptorTableSlot, descriptorHandle->GPU);
		}
		else
		{
			ASSERT_MSG(false, "No pipline bound!");
			return;
		}
	}

	void CommandList_DX12::BindAsRWTexture(u32 slot, Texture texture)
	{
		//ASSERT_MSG(CurrentlyComputeBoundPipeline != nullptr, "A compute shader needs to be bound for this!");

		Texture_DX12* texture_DX12 = (Texture_DX12*)texture.Data.get();

		u32 rootDescriptorTableSlot = 0;
		if (m_BoundPipelineType == PipelineType::Compute)
		{
			TransitionResource(texture_DX12->TextureResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, texture.Desc.NumMips, texture.Desc.NumArraySlices);
			ComputePipeline_DX12* computePipeline_DX12 = (ComputePipeline_DX12*)m_ComputePipeline.Data.get();
			rootDescriptorTableSlot = computePipeline_DX12->UAVIndices[slot];
			CommandBuffer->CommandList->SetComputeRootDescriptorTable(rootDescriptorTableSlot, texture_DX12->uav.GPU);
		}
		else
		{
			ASSERT_MSG(false, "No pipline bound!");
			return;
		}


	}

	void CommandList_DX12::BindAsRWTexture(u32 slot, Texture texture, u32 mipLevel)
	{
		//ASSERT_MSG(CurrentlyComputeBoundPipeline != nullptr, "A compute shader needs to be bound for this!");

		Texture_DX12* texture_DX12 = (Texture_DX12*)texture.Data.get();

		
		u32 rootDescriptorTableSlot = 0;
		if (m_BoundPipelineType == PipelineType::Compute)
		{
			
			TransitionSubResource(
				texture_DX12->TextureResource,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				texture.Desc.NumMips,
				texture.Desc.NumArraySlices,
				mipLevel
			);
			
			ComputePipeline_DX12* computePipeline_DX12 = (ComputePipeline_DX12*)m_ComputePipeline.Data.get();
			rootDescriptorTableSlot = computePipeline_DX12->UAVIndices[slot];
			CommandBuffer->CommandList->SetComputeRootDescriptorTable(rootDescriptorTableSlot, texture_DX12->mipUavs[mipLevel].GPU);
		}
		else
		{
			ASSERT_MSG(false, "No pipline bound!");
			return;
		}
	}

	void CommandList_DX12::BindAsRWTexture(u32 slot, RenderTarget renderTarget, RenderTargetType type)
	{
		//ASSERT_MSG(CurrentlyComputeBoundPipeline != nullptr, "A compute shader needs to be bound for this!");

		RenderTarget_DX12* renderTarget_DX12 = (RenderTarget_DX12*)renderTarget.Data.get();


		DescriptorHandle* descriptorHandle;

		switch (type)
		{
		case RenderTargetType::Target0: descriptorHandle = &renderTarget_DX12->renderTargetUavs[0]; break;
		case RenderTargetType::Target1: descriptorHandle = &renderTarget_DX12->renderTargetUavs[1]; break;
		case RenderTargetType::Target2: descriptorHandle = &renderTarget_DX12->renderTargetUavs[2]; break;
		case RenderTargetType::Target3: descriptorHandle = &renderTarget_DX12->renderTargetUavs[3]; break;
		case RenderTargetType::Target4: descriptorHandle = &renderTarget_DX12->renderTargetUavs[4]; break;
		case RenderTargetType::Target5: descriptorHandle = &renderTarget_DX12->renderTargetUavs[5]; break;
		case RenderTargetType::Target6: descriptorHandle = &renderTarget_DX12->renderTargetUavs[6]; break;
		case RenderTargetType::Target7: descriptorHandle = &renderTarget_DX12->renderTargetUavs[7]; break;
		case RenderTargetType::Target8: descriptorHandle = &renderTarget_DX12->renderTargetUavs[8]; break;
		default:
			ASSERT_MSG(false, "Unkown renderTargetType");
			return;
			break;
		}
		u32 rootDescriptorTableSlot = 0;
		if (m_BoundPipelineType == PipelineType::Compute)
		{
			TransitionRenderTarget(renderTarget, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
			ComputePipeline_DX12* computePipeline_DX12 = (ComputePipeline_DX12*)m_ComputePipeline.Data.get();
			rootDescriptorTableSlot = computePipeline_DX12->UAVIndices[slot];
			CommandBuffer->CommandList->SetComputeRootDescriptorTable(rootDescriptorTableSlot, descriptorHandle->GPU);
		}
		else if (m_BoundPipelineType == PipelineType::Raytracing)
		{
			TransitionRenderTarget(renderTarget, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON);
			RaytracingPipeline_DX12* raytracingPipeline_DX12 = (RaytracingPipeline_DX12*)m_RaytracingPipeline.Data.get();
			rootDescriptorTableSlot = raytracingPipeline_DX12->UAVIndices[slot];
			CommandBuffer->CommandList->SetComputeRootDescriptorTable(rootDescriptorTableSlot, descriptorHandle->GPU);
		}
		else
		{
			ASSERT_MSG(false, "No pipline bound!");
			return;
		}
	}

	void CommandList_DX12::BindGraphicsPipeline(GraphicsPipeline graphicsPipeline)
	{
		GraphicsPipeline_DX12* graphicsPipeline_DX12 = (GraphicsPipeline_DX12*)graphicsPipeline.Data.get();

		CommandBuffer->CommandList->SetPipelineState(graphicsPipeline_DX12->PipelineState.Get());
		CommandBuffer->CommandList->SetGraphicsRootSignature(graphicsPipeline_DX12->RootSignature.Get());

		CommandBuffer->CommandList->IASetPrimitiveTopology(PrimitiveTypeToD3D12PrimitiveTopology(graphicsPipeline.Desc.PrimitiveType));

		m_BoundPipelineType = PipelineType::Graphics;
		m_GraphicsPipeline = graphicsPipeline;
	}
	
	void CommandList_DX12::BindComputePipeline(ComputePipeline computePipeline)
	{
		ComputePipeline_DX12* computePipeline_DX12 = (ComputePipeline_DX12*)computePipeline.Data.get();

		CommandBuffer->CommandList->SetPipelineState(computePipeline_DX12->PipelineState.Get());
		CommandBuffer->CommandList->SetComputeRootSignature(computePipeline_DX12->RootSignature.Get());

		m_BoundPipelineType = PipelineType::Compute;
		m_ComputePipeline = computePipeline;
	}

	void CommandList_DX12::BindRaytracingPipeline(RaytracingPipeline raytracingPipeline)
	{
		RaytracingPipeline_DX12* raytracingPipeline_DX12 = (RaytracingPipeline_DX12*)raytracingPipeline.Data.get();

		CommandBuffer->CommandList->SetPipelineState1(raytracingPipeline_DX12->StateObject.Get());
		CommandBuffer->CommandList->SetComputeRootSignature(raytracingPipeline_DX12->RootSignature.Get());

		m_BoundPipelineType = PipelineType::Raytracing;
		m_RaytracingPipeline = raytracingPipeline;
	}

	void CommandList_DX12::BindConstantBuffer(u32 slot, ConstantBuffer buffer)
	{
		ConstantBuffer_DX12* constantBuffer_DX12 = (ConstantBuffer_DX12*)buffer.Data.get();
		

		u32 rootDescriptorTableSlot = 0;
		if (m_BoundPipelineType == PipelineType::Graphics)
		{
			//TransitionResource(constantBuffer_DX12->ConstantBufferResource, D3D12_RESOURCE_STATE_GENERIC_READ);
			GraphicsPipeline_DX12* graphicsPipeline_DX12 = (GraphicsPipeline_DX12*)m_GraphicsPipeline.Data.get();

			rootDescriptorTableSlot = graphicsPipeline_DX12->ConstantBufferIndices[slot];
			CommandBuffer->CommandList->SetGraphicsRootDescriptorTable(
				rootDescriptorTableSlot,
				constantBuffer_DX12->cbv.GPU
			);
		}
		else if (m_BoundPipelineType == PipelineType::Compute)
		{
			//TransitionResource(constantBuffer_DX12->ConstantBufferResource, D3D12_RESOURCE_STATE_GENERIC_READ);
			ComputePipeline_DX12* computePipeline_DX12 = (ComputePipeline_DX12*)m_ComputePipeline.Data.get();
			rootDescriptorTableSlot = computePipeline_DX12->ConstantBufferIndices[slot];
			CommandBuffer->CommandList->SetComputeRootDescriptorTable(
				rootDescriptorTableSlot,
				constantBuffer_DX12->cbv.GPU
			);
		}
		else if (m_BoundPipelineType == PipelineType::Raytracing)
		{
			//TransitionResource(constantBuffer_DX12->ConstantBufferResource, D3D12_RESOURCE_STATE_GENERIC_READ);
			RaytracingPipeline_DX12* raytracingPipeline_DX12 = (RaytracingPipeline_DX12*)m_RaytracingPipeline.Data.get();
			rootDescriptorTableSlot = raytracingPipeline_DX12->ConstantBufferIndices[slot];
			CommandBuffer->CommandList->SetComputeRootDescriptorTable(
				rootDescriptorTableSlot,
				constantBuffer_DX12->cbv.GPU
			);
		}
		else
		{
			ASSERT_MSG(false, "No pipline bound!");
			return;
		}

		
	}

	void CommandList_DX12::SetDynamicCallData(u32 size, void* data)
	{
		ASSERT_MSG(size % 4 == 0, "The size of dynamic call data must be a multiple of 32 bits or 4 bytes");
		u32 rootDescriptorTableSlot = 0;
		if (m_BoundPipelineType == PipelineType::Graphics)
		{
			GraphicsPipeline_DX12* graphicsPipeline_DX12 = (GraphicsPipeline_DX12*)m_GraphicsPipeline.Data.get();
			ASSERT_MSG(size <= m_GraphicsPipeline.Desc.DynamicCallData.Size, "the size is biggert than the DynamicCallData!");
			ASSERT_MSG(m_GraphicsPipeline.Desc.DynamicCallData.BufferLocation >= 0, "Buffer Location has to be set!");
			rootDescriptorTableSlot = graphicsPipeline_DX12->ConstantBufferIndices[m_GraphicsPipeline.Desc.DynamicCallData.BufferLocation];
			CommandBuffer->CommandList->SetGraphicsRoot32BitConstants(
				rootDescriptorTableSlot,
				size / 4,
				data,
				0
			);
		}
		else if (m_BoundPipelineType == PipelineType::Compute)
		{
			ComputePipeline_DX12* computePipeline_DX12 = (ComputePipeline_DX12*)m_ComputePipeline.Data.get();

			ASSERT_MSG(size <= m_ComputePipeline.Desc.DynamicCallData.Size, "the size is biggert than the DynamicCallData!");
			ASSERT_MSG(m_ComputePipeline.Desc.DynamicCallData.BufferLocation >= 0, "Buffer Location has to be set!");
			rootDescriptorTableSlot = computePipeline_DX12->ConstantBufferIndices[m_ComputePipeline.Desc.DynamicCallData.BufferLocation];
			CommandBuffer->CommandList->SetComputeRoot32BitConstants(
				rootDescriptorTableSlot,
				size / 4,
				data,
				0
			);
		}
		else
		{
			ASSERT_MSG(false, "No pipline bound!");
			return;
		}

		
	}

	void CommandList_DX12::Draw(u32 numIndices)
	{
		CommandBuffer->CommandList->DrawIndexedInstanced(numIndices, 1, 0, 0, 0);
	}
	
	void CommandList_DX12::DrawAuto(u32 numVertices)
	{
		CommandBuffer->CommandList->DrawInstanced(numVertices, 1, 0, 0);
	}

	void CommandList_DX12::BindTLAS(TLAS tlas)
	{
		TLAS_DX12* tlas_DX12 = (TLAS_DX12*)tlas.Data.get();

		u32 rootDescriptorTableSlot = 0;
		if (m_BoundPipelineType == PipelineType::Raytracing)
		{
			RaytracingPipeline_DX12* raytracingPipeline_DX12 = (RaytracingPipeline_DX12*)m_RaytracingPipeline.Data.get();
			rootDescriptorTableSlot = raytracingPipeline_DX12->TLASSlot;
			CommandBuffer->CommandList->SetComputeRootShaderResourceView(rootDescriptorTableSlot, tlas_DX12->Resource->GetGPUVirtualAddress());
		}
		else
		{
			ASSERT_MSG(false, "No pipline bound!");
			return;
		}
	}

	void CommandList_DX12::TransitionRenderTarget(RenderTarget renderTarget, D3D12_RESOURCE_STATES newRenderTargetState, D3D12_RESOURCE_STATES newDepthStencilState)
	{
		RenderTarget_DX12* renderTargetDX12 = (RenderTarget_DX12*)renderTarget.Data.get();

		for (u32 i = 0; i < renderTarget.Desc.NumRenderTargets; i++)
		{
			TransitionResource(
				renderTargetDX12->RenderTargetResources[i],
				newRenderTargetState,
				1,
				1
			);
		}
		if(renderTargetDX12->DepthBufferResource)
		{
			TransitionResource(
				renderTargetDX12->DepthBufferResource,
				newDepthStencilState,
				1,
				1
			);
		}
	}

	void CommandList_DX12::Dispatch(u32 xThreads, u32 yThreads, u32 zThreads)
	{
		CommandBuffer->CommandList->Dispatch(xThreads, yThreads, zThreads);
	}

	void CommandList_DX12::DispatchRays(u32 width, u32 height, u32 depth)
	{

		//Texture_DX12* texture_DX12 = (Texture_DX12*)target->Data.get();

		//CommandBuffer->CommandList->SetComputeRootSignature(raytracingPipeline_DX12->RootSignature.Get());

		//// Bind the heaps, acceleration structure and dispatch rays.    
		//CommandBuffer->CommandList->SetComputeRootDescriptorTable(0, texture_DX12->uav.GPU);


		//RenderTarget_DX12* renderTarget_DX12 = (RenderTarget_DX12*)input->Data.get();

		//DescriptorHandle* descriptorHandlePositions;
		//DescriptorHandle* descriptorHandleNormal;

		//switch (inputPosition)
		//{
		//case RenderTargetType::Target0: descriptorHandlePositions = &renderTarget_DX12->renderTargetSrvs[0]; break;
		//case RenderTargetType::Target1: descriptorHandlePositions = &renderTarget_DX12->renderTargetSrvs[1]; break;
		//case RenderTargetType::Target2: descriptorHandlePositions = &renderTarget_DX12->renderTargetSrvs[2]; break;
		//case RenderTargetType::Target3: descriptorHandlePositions = &renderTarget_DX12->renderTargetSrvs[3]; break;
		//case RenderTargetType::Target4: descriptorHandlePositions = &renderTarget_DX12->renderTargetSrvs[4]; break;
		//case RenderTargetType::Target5: descriptorHandlePositions = &renderTarget_DX12->renderTargetSrvs[5]; break;
		//case RenderTargetType::Target6: descriptorHandlePositions = &renderTarget_DX12->renderTargetSrvs[6]; break;
		//case RenderTargetType::Target7: descriptorHandlePositions = &renderTarget_DX12->renderTargetSrvs[7]; break;
		//case RenderTargetType::Target8: descriptorHandlePositions = &renderTarget_DX12->renderTargetSrvs[8]; break;
		//case RenderTargetType::TargetDepth: descriptorHandlePositions = &renderTarget_DX12->depthStencilSrv; break;
		//default:
		//	ASSERT_MSG(false, "Unkown renderTargetType");
		//	return;
		//	break;
		//}
		//switch (inputNormal)
		//{
		//case RenderTargetType::Target0: descriptorHandleNormal = &renderTarget_DX12->renderTargetSrvs[0]; break;
		//case RenderTargetType::Target1: descriptorHandleNormal = &renderTarget_DX12->renderTargetSrvs[1]; break;
		//case RenderTargetType::Target2: descriptorHandleNormal = &renderTarget_DX12->renderTargetSrvs[2]; break;
		//case RenderTargetType::Target3: descriptorHandleNormal = &renderTarget_DX12->renderTargetSrvs[3]; break;
		//case RenderTargetType::Target4: descriptorHandleNormal = &renderTarget_DX12->renderTargetSrvs[4]; break;
		//case RenderTargetType::Target5: descriptorHandleNormal = &renderTarget_DX12->renderTargetSrvs[5]; break;
		//case RenderTargetType::Target6: descriptorHandleNormal = &renderTarget_DX12->renderTargetSrvs[6]; break;
		//case RenderTargetType::Target7: descriptorHandleNormal = &renderTarget_DX12->renderTargetSrvs[7]; break;
		//case RenderTargetType::Target8: descriptorHandleNormal = &renderTarget_DX12->renderTargetSrvs[8]; break;
		//case RenderTargetType::TargetDepth: descriptorHandleNormal = &renderTarget_DX12->depthStencilSrv; break;
		//default:
		//	ASSERT_MSG(false, "Unkown renderTargetType");
		//	return;
		//	break;
		//}
		//	
		//TransitionRenderTarget(input, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

		//CommandBuffer->CommandList->SetComputeRootDescriptorTable(1, descriptorHandlePositions->GPU);
		//CommandBuffer->CommandList->SetComputeRootDescriptorTable(4, descriptorHandleNormal->GPU);
		//	
		//	
		//CommandBuffer->CommandList->SetComputeRootShaderResourceView(
		//	2, m_topLevelAccelerationStructure->GetGPUVirtualAddress()
		//);

		//ConstantBuffer_DX12* constantBuffer_DX12 = (ConstantBuffer_DX12*)sceneData->Data.get();
		//CommandBuffer->CommandList->SetComputeRootDescriptorTable(
		//	3, constantBuffer_DX12->cbv.GPU
		//);

		RaytracingPipeline_DX12* raytracingPipeline_DX12 = (RaytracingPipeline_DX12*)m_RaytracingPipeline.Data.get();

		D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};

		dispatchDesc.HitGroupTable.StartAddress = raytracingPipeline_DX12->HitGroupShaderTable->GetGPUVirtualAddress();
		dispatchDesc.HitGroupTable.SizeInBytes = raytracingPipeline_DX12->HitGroupShaderTable->GetDesc().Width;
		dispatchDesc.HitGroupTable.StrideInBytes = dispatchDesc.HitGroupTable.SizeInBytes;
		dispatchDesc.MissShaderTable.StartAddress = raytracingPipeline_DX12->MissShaderTable->GetGPUVirtualAddress();
		dispatchDesc.MissShaderTable.SizeInBytes = raytracingPipeline_DX12->MissShaderTable->GetDesc().Width;
		dispatchDesc.MissShaderTable.StrideInBytes = dispatchDesc.MissShaderTable.SizeInBytes;
		dispatchDesc.RayGenerationShaderRecord.StartAddress = raytracingPipeline_DX12->RayGenShaderTable->GetGPUVirtualAddress();
		dispatchDesc.RayGenerationShaderRecord.SizeInBytes = raytracingPipeline_DX12->RayGenShaderTable->GetDesc().Width;
		dispatchDesc.Width = width;
		dispatchDesc.Height = height;
		dispatchDesc.Depth = depth;

		CommandBuffer->CommandList->DispatchRays(&dispatchDesc);

	}


} }


#endif