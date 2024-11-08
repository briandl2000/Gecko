#ifdef WIN32
#include "Objects_DX12.h"

#include "Rendering/DX12/Device_DX12.h"

namespace Gecko { namespace DX12 {

	RenderTarget_DX12::~RenderTarget_DX12()
	{
		for (u32 i = 0; i < 8; i++)
		{
			if (RenderTargetViews[i].IsValid())
			{
				Device_DX12::FlagRtvDescriptorHandleForDeletion(RenderTargetViews[i]);
			}
		}
		Device_DX12::FlagDsvDescriptorHandleForDeletion(DepthStencilView);
	}

	GraphicsPipeline_DX12::~GraphicsPipeline_DX12()
	{
		// device->Flush();
		Device_DX12::FlagPipelineStateForDeletion(PipelineState);
		RootSignature = nullptr;
		PipelineState = nullptr;
	}

	ComputePipeline_DX12::~ComputePipeline_DX12()
	{
		// device->Flush();
		Device_DX12::FlagPipelineStateForDeletion(PipelineState);
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
		Device_DX12::FlagResrouceForDeletion(VertexBufferResource);
	}

	IndexBuffer_DX12::~IndexBuffer_DX12()
	{
		Device_DX12::FlagResrouceForDeletion(IndexBufferResource);
	}

	ConstantBuffer_DX12::~ConstantBuffer_DX12()
	{
		Device_DX12::FlagResrouceForDeletion(ConstantBufferResource);
		Device_DX12::FlagSrvDescriptorHandleForDeletion(cbv);
	}

	Texture_DX12::~Texture_DX12()
	{
		//device->Flush();
		Device_DX12::FlagResrouceForDeletion(TextureResource);
		Device_DX12::FlagSrvDescriptorHandleForDeletion(srv);
		Device_DX12::FlagSrvDescriptorHandleForDeletion(uav);

		for (u32 i = 0; i < mipSrvs.size(); i++)
		{
			Device_DX12::FlagSrvDescriptorHandleForDeletion(mipSrvs[i]);
		}
		for (u32 i = 0; i < mipUavs.size(); i++)
		{
			Device_DX12::FlagSrvDescriptorHandleForDeletion(mipUavs[i]);
		}
	}


} }

#endif