#ifdef WIN32
#include "Objects_DX12.h"

#include "Rendering/DX12/Device_DX12.h"

namespace Gecko { namespace DX12 {

	RenderTarget_DX12::~RenderTarget_DX12()
	{
		ASSERT_MSG(device != nullptr, "device is null, Did you forget to assign it?");

		device->Flush();

		for (u32 i = 0; i < 8; i++)
		{
			if (rtvs[i].IsValid())
			{
				device->GetRtvHeap().Free(rtvs[i]);
				device->GetSrvHeap().Free(renderTargetSrvs[i]);
				device->GetSrvHeap().Free(renderTargetUavs[i]);
			}
		}
		device->GetDsvHeap().Free(dsv);
		device->GetSrvHeap().Free(depthStencilSrv);
	}

	GraphicsPipeline_DX12::~GraphicsPipeline_DX12()
	{
		device->Flush();
		RootSignature = nullptr;
		PipelineState = nullptr;
	}

	ComputePipeline_DX12::~ComputePipeline_DX12()
	{
		device->Flush();
		RootSignature = nullptr;
		PipelineState = nullptr;
	}


	CommandBuffer::~CommandBuffer()
	{
		
		CommandAllocator = nullptr;
		Fence = nullptr;
		CommandList = nullptr;
	}

	VertexBuffer_DX12::~VertexBuffer_DX12()
	{
		device->Flush();
	}

	IndexBuffer_DX12::~IndexBuffer_DX12()
	{
		device->Flush();
	}

	ConstantBuffer_DX12::~ConstantBuffer_DX12()
	{
		device->Flush();
		device->GetSrvHeap().Free(cbv);
	}

	Texture_DX12::~Texture_DX12()
	{
		device->Flush();
		device->GetSrvHeap().Free(srv);
		device->GetSrvHeap().Free(uav);

		for (u32 i = 0; i < mipSrvs.size(); i++)
		{
			device->GetSrvHeap().Free(mipSrvs[i]);
		}
		for (u32 i = 0; i < mipUavs.size(); i++)
		{
			device->GetSrvHeap().Free(mipUavs[i]);
		}
	}


} }

#endif