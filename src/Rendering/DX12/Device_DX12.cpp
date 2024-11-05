#ifdef WIN32

#include "Rendering/DX12/Device_DX12.h"
#include "Core/Platform.h"
#include "CommandList_DX12.h"

#include "Objects_DX12.h"

#include <dxgi1_6.h>
#include "d3dx12.h"
#include <d3dcompiler.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx12.h>

#define SizeOfInUint32(obj) ((sizeof(obj) - 1) / sizeof(UINT32) + 1)
#if defined(min)
#undef min
#endif
#if defined(max)
#undef max
#endif

namespace Gecko { namespace DX12
{

	// Device Interface Methods

#pragma region Device Interface Methods
	
	void Device_DX12::Init()
	{

		// Add the resize event
		AddEventListener(Event::SystemEvent::CODE_RESIZED, &Device_DX12::Resize);

		const Platform::AppInfo& info = Platform::GetAppInfo();
		m_NumBackBuffers = info.NumBackBuffers;

		// Create the device
		CreateDevice();

		// Create the command queue, allocator and list for the Graphics
		CreateCommandBuffers(m_Device, &m_GraphicsCommandQueue, &m_GraphicsCommandBuffers, D3D12_COMMAND_LIST_TYPE_DIRECT);
		CreateCommandBuffers(m_Device, &m_ComputeCommandQueue, &m_ComputeCommandBuffers, D3D12_COMMAND_LIST_TYPE_COMPUTE);
		CreateCommandBuffers(m_Device, &m_CopyCommandQueue, &m_CopyCommandBuffers, D3D12_COMMAND_LIST_TYPE_COPY);

		// Create a fence
		m_FenceEvent = CreateEventEx(nullptr, nullptr, 0, EVENT_ALL_ACCESS);
		ASSERT(m_FenceEvent);

		// Create Discriptor heaps
		{
			bool result = true;
			result &= m_RtvDescHeap.Initialize(this, 512, false);
			result &= m_DsvDescHeap.Initialize(this, 512, false);
			result &= m_SrvDescHeap.Initialize(this, 4096, true);
			result &= m_UavDescHeap.Initialize(this, 512, false);
			ASSERT_MSG(result, "Failed to initialize heaps!");
		}

		// Creating the swap chain
		DXGI_SWAP_CHAIN_DESC1 swapchainDesc{};
		m_BackBufferFormat = Format::R8G8B8A8_UNORM;
		swapchainDesc.Width = info.Width;
		swapchainDesc.Height = info.Height;
		swapchainDesc.Format = FormatToD3D12Format(m_BackBufferFormat);
		swapchainDesc.Stereo = false;
		swapchainDesc.SampleDesc.Count = 1;
		swapchainDesc.SampleDesc.Quality = 0;
		swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapchainDesc.BufferCount = m_NumBackBuffers;
		swapchainDesc.Scaling = DXGI_SCALING_STRETCH;
		swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
		swapchainDesc.Flags = 0;

		ComPtr<IDXGISwapChain1> swapChain;
		HWND window = reinterpret_cast<HWND>(Platform::GetWindowData());
		DIRECTX12_ASSERT(m_Factory->CreateSwapChainForHwnd(m_GraphicsCommandQueue.Get(), window, &swapchainDesc, nullptr, nullptr, &swapChain));
		DIRECTX12_ASSERT(m_Factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER));
		DIRECTX12_ASSERT(swapChain->QueryInterface(IID_PPV_ARGS(&m_SwapChain)));

		// Createing the back buffer render targets
		RecreateBackBuffers(info.Width, info.Height);

		// Setting up ImGui
		// ImGui stuff
		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;       // Enable Keyboard Controls
		//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
		//io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;           // Enable Docking
		io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;         // Enable Multi-Viewport / Platform Windows
		//io.ConfigViewportsNoAutoMerge = true;
		//io.ConfigViewportsNoTaskBarIcon = true;
		io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

		// Setup Dear ImGui style
		ImGui::StyleColorsDark();
		//ImGui::StyleColorsLight();

		// When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular ones.
		ImGuiStyle& style = ImGui::GetStyle();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}

		ImGui_ImplWin32_Init(window);
		m_ImGuiHandle = m_SrvDescHeap.Allocate();
		ImGui_ImplDX12_Init(
			m_Device.Get(),
			info.NumBackBuffers,
			FormatToD3D12Format(m_BackBufferFormat),
			m_SrvDescHeap.Heap(),
			m_ImGuiHandle.CPU,
			m_ImGuiHandle.GPU
		);

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();

	}

	void Device_DX12::Shutdown()
	{
		//ShutdownRaytracing();

		Flush();

		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();

		CloseHandle(m_FenceEvent);
		m_NumBackBuffers = 0;

		m_BackBuffers.clear();

		m_RtvDescHeap.Destroy();
		m_DsvDescHeap.Destroy();
		m_SrvDescHeap.Destroy();
		m_UavDescHeap.Destroy();

		for (u32 i = 0; i < m_GraphicsCommandBuffers.size(); i++)
		{
			m_GraphicsCommandBuffers[i] = nullptr;
		}

		for (u32 i = 0; i < m_ComputeCommandBuffers.size(); i++)
		{
			m_ComputeCommandBuffers[i] = nullptr;
		}

		for (u32 i = 0; i < m_CopyCommandBuffers.size(); i++)
		{
			m_CopyCommandBuffers[i] = nullptr;
		}

		m_GraphicsCommandQueue = nullptr;
		m_ComputeCommandQueue = nullptr;
		m_CopyCommandQueue = nullptr;
		m_SwapChain = nullptr;
		m_Factory = nullptr;
		m_FenceEvent = nullptr;
		m_Device = nullptr;

#ifdef DEBUG
		{
			ComPtr<IDXGIDebug1> debugDevice;
			DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debugDevice));
			debugDevice->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
		}
#endif

		RemoveEventListener(Event::SystemEvent::CODE_RESIZED, &Device_DX12::Resize);

	}

	Ref<CommandList> Device_DX12::CreateGraphicsCommandList()
	{
		Ref<CommandBuffer> graphicsCommandBuffer = GetFreeGraphicsCommandBuffer();
		Ref<CommandList_DX12> commandList = CreateRef<CommandList_DX12>();
		commandList->CommandBuffer = graphicsCommandBuffer;
		
		ID3D12DescriptorHeap* descriptorHeaps[] = { m_SrvDescHeap.Heap()};
		commandList->CommandBuffer->CommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		return commandList;
	}

	void Device_DX12::ExecuteGraphicsCommandList(Ref<CommandList> commandList)
	{
		CommandList_DX12& commandListDx12 = *(CommandList_DX12*)commandList.get();
		

		DIRECTX12_ASSERT(commandListDx12.CommandBuffer->CommandList->Close());
		ID3D12CommandList* const commandLists[]{ commandListDx12.CommandBuffer->CommandList.Get()};
		m_GraphicsCommandQueue->ExecuteCommandLists(_countof(commandLists), &commandLists[0]);

		u64 fenceValueForSignal = ++commandListDx12.CommandBuffer->FenceValue;
		m_GraphicsCommandQueue->Signal(commandListDx12.CommandBuffer->Fence.Get(), fenceValueForSignal);
		commandListDx12.CommandBuffer = nullptr;

	}
	
	void Device_DX12::ExecuteGraphicsCommandListAndFlip(Ref<CommandList> commandList)
	{

		CommandList_DX12& commandListDx12 = *(CommandList_DX12*)commandList.get();
		commandListDx12.TransitionRenderTarget(GetCurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COMMON);

		ExecuteGraphicsCommandList(commandList);

		ASSERT(m_SwapChain);
		DIRECTX12_ASSERT(m_SwapChain->Present(0, 0));
		m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();

		ProcessDeferredReleases();
	}

	Ref<CommandList> Device_DX12::CreateComputeCommandList()
	{
		Ref<CommandBuffer> computeCommandBuffer = GetFreeComputeCommandBuffer();
		Ref<CommandList_DX12> commandList = CreateRef<CommandList_DX12>();
		commandList->CommandBuffer = computeCommandBuffer;

		ID3D12DescriptorHeap* descriptorHeaps[] = { m_SrvDescHeap.Heap() };
		commandList->CommandBuffer->CommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		return commandList;
	}
	
	void Device_DX12::ExecuteComputeCommandList(Ref<CommandList> commandList)
	{
		CommandList_DX12& commandListDx12 = *(CommandList_DX12*)commandList.get();

		DIRECTX12_ASSERT(commandListDx12.CommandBuffer->CommandList->Close());
		ID3D12CommandList* const commandLists[]{ commandListDx12.CommandBuffer->CommandList.Get() };
		m_ComputeCommandQueue->ExecuteCommandLists(_countof(commandLists), &commandLists[0]);

		u64 fenceValueForSignal = ++commandListDx12.CommandBuffer->FenceValue;
		m_ComputeCommandQueue->Signal(commandListDx12.CommandBuffer->Fence.Get(), fenceValueForSignal);
		commandListDx12.CommandBuffer = nullptr;
	}

	RenderTarget Device_DX12::CreateRenderTarget(const RenderTargetDesc& desc)
	{
		Ref<RenderTarget_DX12> renderTargetDX12 = CreateRef<RenderTarget_DX12>();
		RenderTarget renderTarget;
		for (u32 i = 0; i < desc.NumRenderTargets; i++)
		{
		
			ASSERT_MSG(desc.RenderTargetFormats[i] != Format::None, "None is not a valid format for a render target, did you forget to set it?");

			DXGI_FORMAT format = FormatToD3D12Format(desc.RenderTargetFormats[i]);

			D3D12_CLEAR_VALUE clearValue;
			clearValue.Format = format;
			clearValue.Color[0] = desc.RenderTargetClearValues[i].Values[0];
			clearValue.Color[1] = desc.RenderTargetClearValues[i].Values[1];
			clearValue.Color[2] = desc.RenderTargetClearValues[i].Values[2];
			clearValue.Color[3] = desc.RenderTargetClearValues[i].Values[3];

			u32 numTextureMips = std::min(desc.NumMips[i], CalculateNumberOfMips(desc.Width, desc.Height));
			TextureDesc textureDesc;
			textureDesc.Name = desc.Name;
			textureDesc.Width = desc.Width;
			textureDesc.Height = desc.Height;
			textureDesc.Depth = 1;
			textureDesc.Format = desc.RenderTargetFormats[i];
			textureDesc.NumMips = numTextureMips;
			textureDesc.Type = TextureType::Tex2D;
			
			renderTarget.RenderTextures[i] = CreateTexture(textureDesc, FormatToD3D12Format(desc.RenderTargetFormats[i]), D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES, &clearValue);

			Texture_DX12* textureDX12 = (Texture_DX12*)renderTarget.RenderTextures[i].Data.get();

			renderTargetDX12->RenderTargetViews[i] = GetRtvHeap().Allocate();

			D3D12_RENDER_TARGET_VIEW_DESC renderTargetDesc{};
			renderTargetDesc.Format = format;
			renderTargetDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			m_Device->CreateRenderTargetView(textureDX12->TextureResource->ResourceDX12.Get(), &renderTargetDesc, renderTargetDX12->RenderTargetViews[i].CPU);
		}

		if(desc.DepthStencilFormat != Format::None)
		{	
			D3D12_CLEAR_VALUE clearValue;
			clearValue.Format = DXGI_FORMAT_D32_FLOAT;
			clearValue.DepthStencil.Depth = desc.DepthTargetClearValue.Depth;
			clearValue.DepthStencil.Stencil = 0;

			u32 numTextureMips = std::min(desc.DepthMips, CalculateNumberOfMips(desc.Width, desc.Height));
			TextureDesc textureDesc;
			textureDesc.Name = "Depth";
			textureDesc.Width = desc.Width;
			textureDesc.Height = desc.Height;
			textureDesc.Depth = 1;
			textureDesc.Format = desc.DepthStencilFormat;
			textureDesc.NumMips = numTextureMips;
			textureDesc.Type = TextureType::Tex2D;

			renderTarget.DepthTexture = CreateTexture(textureDesc, DXGI_FORMAT_D32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL, D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES, &clearValue);
		
			Texture_DX12* textureDX12 = (Texture_DX12*)renderTarget.DepthTexture.Data.get();

			renderTargetDX12->DepthStencilView = GetDsvHeap().Allocate();

			D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc;
			depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
			depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			depthStencilDesc.Texture2D.MipSlice = 0;
			depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;
			m_Device->CreateDepthStencilView(textureDX12->TextureResource->ResourceDX12.Get(), &depthStencilDesc, renderTargetDX12->DepthStencilView.CPU);
			//NAME_DIRECTX12_OBJECT(resource->ResourceDX12, "DepthBuffer");
		}


		renderTargetDX12->rect.left = 0;
		renderTargetDX12->rect.top = 0;
		renderTargetDX12->rect.right = (i32)desc.Width;
		renderTargetDX12->rect.bottom = (i32)desc.Height;

		renderTargetDX12->ViewPort.TopLeftX = 0.f;
		renderTargetDX12->ViewPort.TopLeftY = 0.f;
		renderTargetDX12->ViewPort.Width = (float)desc.Width;
		renderTargetDX12->ViewPort.Height = (float)desc.Height;
		renderTargetDX12->ViewPort.MinDepth = 0.f;
		renderTargetDX12->ViewPort.MaxDepth = 1.f;
		
		renderTargetDX12->device = this;

		renderTarget.Desc = desc;
		renderTarget.Data = renderTargetDX12;

		return renderTarget;
	}

	VertexBuffer Device_DX12::CreateVertexBuffer(const VertexBufferDesc& desc)
	{
		Ref<VertexBuffer_DX12> vertexBuffer_DX12 = CreateRef<VertexBuffer_DX12>();
		
		vertexBuffer_DX12->device = this;

		size_t bufferSize = desc.NumVertices * desc.Layout.Stride;

		CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
		CD3DX12_RESOURCE_DESC buffer = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, D3D12_RESOURCE_FLAG_NONE);
		vertexBuffer_DX12->VertexBufferResource = CreateRef<Resource>();

		m_Device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&buffer,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&vertexBuffer_DX12->VertexBufferResource->ResourceDX12)
		);

		D3D12_SUBRESOURCE_DATA subresourceData = {};
		subresourceData.pData = desc.VertexData;
		subresourceData.RowPitch = bufferSize;
		subresourceData.SlicePitch = subresourceData.RowPitch;

		CopyToResource(vertexBuffer_DX12->VertexBufferResource->ResourceDX12, subresourceData);

		// Create the vertex buffer view.
		vertexBuffer_DX12->VertexBufferView.BufferLocation = vertexBuffer_DX12->VertexBufferResource->ResourceDX12->GetGPUVirtualAddress();
		vertexBuffer_DX12->VertexBufferView.SizeInBytes = static_cast<u32>(bufferSize);
		vertexBuffer_DX12->VertexBufferView.StrideInBytes = desc.Layout.Stride;

		VertexBuffer vertexBuffer;
		vertexBuffer.Desc = desc;
		vertexBuffer.Data = vertexBuffer_DX12;
		

		return vertexBuffer;
	}

	IndexBuffer Device_DX12::CreateIndexBuffer(const IndexBufferDesc& desc)
	{
		Ref<IndexBuffer_DX12> indexBuffer_DX12 = CreateRef<IndexBuffer_DX12>();

		indexBuffer_DX12->device = this;

		size_t bufferSize = desc.NumIndices * FormatSizeInBytes(desc.IndexFormat);

		CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_DEFAULT);
		CD3DX12_RESOURCE_DESC buffer = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, D3D12_RESOURCE_FLAG_NONE);
		indexBuffer_DX12->IndexBufferResource = CreateRef<Resource>();
		m_Device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&buffer,
			D3D12_RESOURCE_STATE_COMMON,
			nullptr,
			IID_PPV_ARGS(&indexBuffer_DX12->IndexBufferResource->ResourceDX12)
		);

		NAME_DIRECTX12_OBJECT(indexBuffer_DX12->IndexBufferResource->ResourceDX12, "IndexBuffer");

		D3D12_SUBRESOURCE_DATA subresourceData = {};
		subresourceData.pData = desc.IndexData;
		subresourceData.RowPitch = bufferSize;
		subresourceData.SlicePitch = subresourceData.RowPitch;

		CopyToResource(indexBuffer_DX12->IndexBufferResource->ResourceDX12, subresourceData);

		indexBuffer_DX12->IndexBufferView.BufferLocation = indexBuffer_DX12->IndexBufferResource->ResourceDX12->GetGPUVirtualAddress();
		indexBuffer_DX12->IndexBufferView.SizeInBytes = static_cast<u32>(bufferSize);
		indexBuffer_DX12->IndexBufferView.Format = FormatToD3D12Format(desc.IndexFormat);

		IndexBuffer indexBuffer;
		indexBuffer.Desc = desc;
		indexBuffer.Data = indexBuffer_DX12;

		return indexBuffer;
	}
	
	GraphicsPipeline Device_DX12::CreateGraphicsPipeline(const GraphicsPipelineDesc& desc)
	{

		Ref<GraphicsPipeline_DX12> graphicsPipeline_DX12 = CreateRef<GraphicsPipeline_DX12>();

		graphicsPipeline_DX12->device = this;


		CD3DX12_PIPELINE_STATE_STREAM2 piplineStateStream2;
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
		{
			D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
			if (m_Device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, static_cast<u32>(sizeof(featureData))))
			{
				featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
			}

			std::vector<CD3DX12_ROOT_PARAMETER1> rootParameters;
			std::vector<D3D12_DESCRIPTOR_RANGE1> descriptorTableRanges;

			u32 numConstantBuffers = static_cast<u32>(desc.ConstantBufferVisibilities.size());
			u32 numTextures = static_cast<u32>(desc.TextureShaderVisibilities.size());

			u32 numRootParams = numConstantBuffers + numTextures;
			if (desc.DynamicCallData.BufferLocation >= 0)
			{
				numRootParams += 1;
			}
			u32 TextureOffset = numConstantBuffers;
			u32 DynamicCallDataOffset = TextureOffset + numTextures;
			rootParameters.resize(numRootParams);
			u32 numDescriptorTableRanges = numConstantBuffers + numTextures;
			descriptorTableRanges.resize(numDescriptorTableRanges);


			for (u32 i = 0; i < numConstantBuffers; i++)
			{
				u32 descriptorIndex = i;
				const ShaderVisibility& constantBufferVisibility = desc.ConstantBufferVisibilities[i];

				descriptorTableRanges[descriptorIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
				descriptorTableRanges[descriptorIndex].NumDescriptors = 1;
				descriptorTableRanges[descriptorIndex].BaseShaderRegister = i;
				descriptorTableRanges[descriptorIndex].RegisterSpace = 0;
				descriptorTableRanges[descriptorIndex].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

				D3D12_ROOT_DESCRIPTOR_TABLE1 descriptorTable;
				descriptorTable.NumDescriptorRanges = 1;
				descriptorTable.pDescriptorRanges = &descriptorTableRanges[descriptorIndex];

				rootParameters[descriptorIndex].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				rootParameters[descriptorIndex].DescriptorTable = descriptorTable;
				rootParameters[descriptorIndex].ShaderVisibility = ShaderVisibilityToD3D12ShaderVisibility(constantBufferVisibility);
				graphicsPipeline_DX12->ConstantBufferIndices.push_back(descriptorIndex);
			}

			for (u32 i = 0; i < numTextures; i++)
			{
				u32 descriptorIndex = i + TextureOffset;
				const ShaderVisibility& textureShaderVisibility = desc.TextureShaderVisibilities[i];

				descriptorTableRanges[descriptorIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
				descriptorTableRanges[descriptorIndex].NumDescriptors = 1;
				descriptorTableRanges[descriptorIndex].BaseShaderRegister = i;
				descriptorTableRanges[descriptorIndex].RegisterSpace = 0;
				descriptorTableRanges[descriptorIndex].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

				/*D3D12_ROOT_DESCRIPTOR_TABLE1 descriptorTable;
				descriptorTable.NumDescriptorRanges = 1;
				descriptorTable.pDescriptorRanges = &descriptorTableRanges[descriptorIndex];*/

				rootParameters[descriptorIndex].InitAsDescriptorTable(
					1,
					&descriptorTableRanges[descriptorIndex],
					ShaderVisibilityToD3D12ShaderVisibility(textureShaderVisibility)
				);

				/*rootParameters[descriptorIndex].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				rootParameters[descriptorIndex].DescriptorTable = descriptorTable;
				rootParameters[descriptorIndex].ShaderVisibility = ShaderVisibilityToD3D12ShaderVisibility(textureShaderVisibility);*/
				graphicsPipeline_DX12->TextureIndices.push_back(descriptorIndex);
			}

			if (desc.DynamicCallData.BufferLocation >= 0)
			{
				ASSERT_MSG(desc.DynamicCallData.Size > 0, "The Size of the dynamic call data must be greater than 0");

				u32 descriptorIndex = DynamicCallDataOffset;
				rootParameters[descriptorIndex].InitAsConstants(
					desc.DynamicCallData.Size / 4,
					desc.DynamicCallData.BufferLocation,
					0,
					ShaderVisibilityToD3D12ShaderVisibility(desc.DynamicCallData.ConstantBufferVisibilities)
				);
				graphicsPipeline_DX12->ConstantBufferIndices.push_back(descriptorIndex);
			}

			std::vector<CD3DX12_STATIC_SAMPLER_DESC> samplers;
			samplers.resize(desc.SamplerDescs.size());
			for (u32 i = 0; i < desc.SamplerDescs.size(); i++)
			{
				samplers[i] = CD3DX12_STATIC_SAMPLER_DESC(
					i,
					SamplerFilterToD3D12Filter(desc.SamplerDescs[i].Filter),
					AddressModeToD3D12AddressMode(desc.SamplerDescs[i].WrapU),
					AddressModeToD3D12AddressMode(desc.SamplerDescs[i].WrapV),
					AddressModeToD3D12AddressMode(desc.SamplerDescs[i].WrapW)
				);
			}

			D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
				D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

			CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
			rootSignatureDescription.Init_1_1(
				static_cast<u32>(rootParameters.size()),
				rootParameters.data(),
				static_cast<u32>(desc.SamplerDescs.size()),
				samplers.data(),
				rootSignatureFlags
			);

			// Serialize the root signature.
			ComPtr<ID3DBlob> rootSignatureBlob;
			ComPtr<ID3DBlob> errorBlob;
			DIRECTX12_ASSERT(D3DX12SerializeVersionedRootSignature(
				&rootSignatureDescription,
				featureData.HighestVersion,
				&rootSignatureBlob,
				&errorBlob
			));
			// Create the root signature.
			DIRECTX12_ASSERT(m_Device->CreateRootSignature(
				0,
				rootSignatureBlob->GetBufferPointer(),
				rootSignatureBlob->GetBufferSize(),
				IID_PPV_ARGS(&graphicsPipeline_DX12->RootSignature)
			));

			pRootSignature = graphicsPipeline_DX12->RootSignature.Get();
			//pipelineCreation.AddPipelineSubObj(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE, &pRootSignature);
		}

		piplineStateStream2.pRootSignature = pRootSignature;

		CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
		std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayoutElements;
		{
			for (const VertexAttribute& vertexAttrib : desc.VertexLayout.Attributes)
			{
				D3D12_INPUT_ELEMENT_DESC elementDesc;

				elementDesc.SemanticName = vertexAttrib.Name;
				elementDesc.SemanticIndex = 0;
				elementDesc.Format = FormatToD3D12Format(vertexAttrib.Format);
				elementDesc.InputSlot = 0;
				elementDesc.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
				elementDesc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
				elementDesc.InstanceDataStepRate = 0;
				inputLayoutElements.push_back(elementDesc);
			}

			InputLayout = { inputLayoutElements.data(), static_cast<u32>(inputLayoutElements.size()) };

		}
		piplineStateStream2.InputLayout = InputLayout;

		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
		{
			PrimitiveTopologyType = PrimitiveTypeToD3D12PrimitiveTopologyType(desc.PrimitiveType);
		}
		piplineStateStream2.PrimitiveTopologyType = PrimitiveTopologyType;

		CD3DX12_PIPELINE_STATE_STREAM_VS VS;
		ComPtr<ID3DBlob> vertexShaderBlob;
		if (desc.VertexShaderPath != nullptr)
		{
			std::string vertexPath = Platform::GetLocalPath(desc.VertexShaderPath);
			std::wstring vertexSource;
			vertexSource.assign(vertexPath.begin(), vertexPath.end());

			ComPtr<ID3DBlob> errorBlob;
			UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
			flags |= D3DCOMPILE_DEBUG;
#endif

			std::string shaderVersion = "vs_";
			shaderVersion += desc.ShaderVersion;
			D3D_SHADER_MACRO macros[] = { "HLSL", "1", "VERTEX", "1", NULL, NULL };
			HRESULT hr = D3DCompileFromFile(vertexSource.c_str(), macros, D3D_COMPILE_STANDARD_FILE_INCLUDE,
				desc.VertexEntryPoint, shaderVersion.c_str(), flags, 0, &vertexShaderBlob, &errorBlob);
			// Should probably do some nicer error handling here #FIXME
			ASSERT(hr == S_OK);

			VS = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
			piplineStateStream2.VS = VS;
		}

		CD3DX12_PIPELINE_STATE_STREAM_PS PS;
		ComPtr<ID3DBlob> pixelShaderBlob;
		if (desc.PixelShaderPath != nullptr)
		{
			std::string pixelPath = Platform::GetLocalPath(desc.PixelShaderPath);
			std::wstring pixelSource;
			pixelSource.assign(pixelPath.begin(), pixelPath.end());

			ComPtr<ID3DBlob> errorBlob;
			UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
			flags |= D3DCOMPILE_DEBUG;
#endif

			std::string shaderVersion = "ps_";
			shaderVersion += desc.ShaderVersion;
			D3D_SHADER_MACRO macros[] = { "HLSL", "1", "PIXEL", "1", NULL, NULL };
			HRESULT hr = D3DCompileFromFile(pixelSource.c_str(), macros, D3D_COMPILE_STANDARD_FILE_INCLUDE,
				desc.PixelEntryPoint, shaderVersion.c_str(), flags, 0, &pixelShaderBlob, &errorBlob);
			// Should probably do some nicer error handling here #FIXME
			ASSERT(hr == S_OK);

			PS = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
			piplineStateStream2.PS = PS;
		}

		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
		D3D12_RT_FORMAT_ARRAY rtvFormats = { {DXGI_FORMAT_UNKNOWN}, 1 };
		{

			u32 i = 0;
			for (; i < 8; i++)
			{
				if (desc.RenderTargetFormats[i] == Format::None)
					break;

				rtvFormats.RTFormats[i] = FormatToD3D12Format(desc.RenderTargetFormats[i]);
			}
			rtvFormats.NumRenderTargets = i;
			RTVFormats = rtvFormats;
		}
		if (rtvFormats.NumRenderTargets > 0)
		{
			piplineStateStream2.RTVFormats = RTVFormats;
		}

		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormats;
		CD3DX12_DEPTH_STENCIL_DESC1 DepthStencilState;
		if (desc.DepthStencilFormat != Format::None)
		{
			DXGI_FORMAT dsvFormat = DXGI_FORMAT_D32_FLOAT;
			DSVFormats = dsvFormat;
			DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC1(CD3DX12_DEFAULT());
			DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		}
		else {
			DXGI_FORMAT dsvFormat = DXGI_FORMAT_UNKNOWN;
			DSVFormats = dsvFormat;
			DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC1();
			DepthStencilState.DepthEnable = false;
		}
		piplineStateStream2.DSVFormat = DSVFormats;
		piplineStateStream2.DepthStencilState = DepthStencilState;

		CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
		{
			Rasterizer = CD3DX12_RASTERIZER_DESC(
				D3D12_FILL_MODE_SOLID, // GetD3D12FillModeFromPrimitiveType(desc.PrimitiveType),
				CullModeToDirectX12CullMode(desc.CullMode),
				desc.WindingOrder == WindingOrder::CounterClockWise ? true : false,
				D3D12_DEFAULT_DEPTH_BIAS,
				D3D12_DEFAULT_DEPTH_BIAS_CLAMP,
				D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS,
				TRUE,
				FALSE,
				FALSE,
				0,
				D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF
			);
		}
		piplineStateStream2.RasterizerState = Rasterizer;

		D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsDesc = piplineStateStream2.GraphicsDescV0();

		HRESULT hr = m_Device->CreateGraphicsPipelineState(&graphicsDesc, IID_PPV_ARGS(&graphicsPipeline_DX12->PipelineState));

		ASSERT(hr == S_OK);

		GraphicsPipeline graphicsPipeline;
		graphicsPipeline.Desc = desc;
		graphicsPipeline.Data = graphicsPipeline_DX12;

		return graphicsPipeline;
	}

	ComputePipeline Device_DX12::CreateComputePipeline(const ComputePipelineDesc& desc)
	{

		Ref<ComputePipeline_DX12> computePipeline_DX12 = CreateRef<ComputePipeline_DX12>();

		computePipeline_DX12->device = this;

		D3D12_COMPUTE_PIPELINE_STATE_DESC computeStateDesc;
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
		{
			D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
			featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
			if (m_Device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, static_cast<u32>(sizeof(featureData))))
			{
				featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
			}

			std::vector<CD3DX12_ROOT_PARAMETER1> rootParameters;
			std::vector<D3D12_DESCRIPTOR_RANGE1> descriptorTableRanges;

			u32 numRootParams = desc.NumConstantBuffers + desc.NumTextures + desc.NumUAVs;
			if (desc.DynamicCallData.BufferLocation >= 0)
			{
				numRootParams += 1;
			}
			u32 TextureOffset = desc.NumConstantBuffers;
			u32 UAVOffset = TextureOffset + desc.NumTextures;
			u32 DynamicCallDataOffset = UAVOffset + desc.NumUAVs;
			rootParameters.resize(numRootParams);
			u32 numDescriptorTableRanges = desc.NumConstantBuffers + desc.NumTextures + desc.NumUAVs;
			descriptorTableRanges.resize(numDescriptorTableRanges);


			for (u32 i = 0; i < desc.NumConstantBuffers; i++)
			{
				u32 descriptorIndex = i;

				descriptorTableRanges[descriptorIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
				descriptorTableRanges[descriptorIndex].NumDescriptors = 1;
				descriptorTableRanges[descriptorIndex].BaseShaderRegister = i;
				descriptorTableRanges[descriptorIndex].RegisterSpace = 0;
				descriptorTableRanges[descriptorIndex].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

				D3D12_ROOT_DESCRIPTOR_TABLE1 descriptorTable;
				descriptorTable.NumDescriptorRanges = 1;
				descriptorTable.pDescriptorRanges = &descriptorTableRanges[descriptorIndex];

				rootParameters[descriptorIndex].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				rootParameters[descriptorIndex].DescriptorTable = descriptorTable;
				rootParameters[descriptorIndex].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
				computePipeline_DX12->ConstantBufferIndices.push_back(descriptorIndex);
			}

			for (u32 i = 0; i < desc.NumTextures; i++)
			{
				u32 descriptorIndex = i + TextureOffset;

				descriptorTableRanges[descriptorIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
				descriptorTableRanges[descriptorIndex].NumDescriptors = 1;
				descriptorTableRanges[descriptorIndex].BaseShaderRegister = i;
				descriptorTableRanges[descriptorIndex].RegisterSpace = 0;
				descriptorTableRanges[descriptorIndex].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

				rootParameters[descriptorIndex].InitAsDescriptorTable(
					1,
					&descriptorTableRanges[descriptorIndex],
					D3D12_SHADER_VISIBILITY_ALL
				);

				computePipeline_DX12->TextureIndices.push_back(descriptorIndex);
			}


			for (u32 i = 0; i < desc.NumUAVs; i++)
			{
				u32 descriptorIndex = i + UAVOffset;

				descriptorTableRanges[descriptorIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
				descriptorTableRanges[descriptorIndex].NumDescriptors = 1;
				descriptorTableRanges[descriptorIndex].BaseShaderRegister = i;
				descriptorTableRanges[descriptorIndex].RegisterSpace = 0;
				descriptorTableRanges[descriptorIndex].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

				D3D12_ROOT_DESCRIPTOR_TABLE1 descriptorTable;
				descriptorTable.NumDescriptorRanges = 1;
				descriptorTable.pDescriptorRanges = &descriptorTableRanges[descriptorIndex];

				rootParameters[descriptorIndex].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
				rootParameters[descriptorIndex].DescriptorTable = descriptorTable;
				rootParameters[descriptorIndex].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
				computePipeline_DX12->UAVIndices.push_back(descriptorIndex);
			}

			if (desc.DynamicCallData.BufferLocation >= 0)
			{
				ASSERT_MSG(desc.DynamicCallData.Size > 0, "The Size of the dynamic call data must be greater than 0");

				u32 descriptorIndex = DynamicCallDataOffset;
				rootParameters[descriptorIndex].InitAsConstants(
					desc.DynamicCallData.Size / 4,
					desc.DynamicCallData.BufferLocation,
					0,
					D3D12_SHADER_VISIBILITY_ALL
				);
				computePipeline_DX12->ConstantBufferIndices.push_back(descriptorIndex);
			}


			std::vector<CD3DX12_STATIC_SAMPLER_DESC> samplers;
			samplers.resize(desc.SamplerDescs.size());
			for (u32 i = 0; i < desc.SamplerDescs.size(); i++)
			{
				samplers[i] = CD3DX12_STATIC_SAMPLER_DESC(
					i,
					SamplerFilterToD3D12Filter(desc.SamplerDescs[i].Filter),
					AddressModeToD3D12AddressMode(desc.SamplerDescs[i].WrapU),
					AddressModeToD3D12AddressMode(desc.SamplerDescs[i].WrapV),
					AddressModeToD3D12AddressMode(desc.SamplerDescs[i].WrapW)
				);
			}

			D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
				D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

			CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDescription;
			rootSignatureDescription.Init_1_1(
				static_cast<u32>(rootParameters.size()),
				rootParameters.data(),
				static_cast<u32>(desc.SamplerDescs.size()),
				samplers.data(),
				rootSignatureFlags
			);

			// Serialize the root signature.
			ComPtr<ID3DBlob> rootSignatureBlob;
			ComPtr<ID3DBlob> errorBlob;
			DIRECTX12_ASSERT(D3DX12SerializeVersionedRootSignature(
				&rootSignatureDescription,
				featureData.HighestVersion,
				&rootSignatureBlob,
				&errorBlob
			));
			// Create the root signature.
			DIRECTX12_ASSERT(m_Device->CreateRootSignature(
				0,
				rootSignatureBlob->GetBufferPointer(),
				rootSignatureBlob->GetBufferSize(),
				IID_PPV_ARGS(&computePipeline_DX12->RootSignature)
			));

			pRootSignature = computePipeline_DX12->RootSignature.Get();
		}

		computeStateDesc.pRootSignature = pRootSignature;

		CD3DX12_PIPELINE_STATE_STREAM_CS CS;
		ComPtr<ID3DBlob> computeShaderBlob;
		{
			std::string computePath = Platform::GetLocalPath(desc.ComputeShaderPath);
			std::wstring ComputeSource;
			ComputeSource.assign(computePath.begin(), computePath.end());

			ComPtr<ID3DBlob> errorBlob;
			UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DEBUG ) || defined( _DEBUG )
			flags |= D3DCOMPILE_DEBUG;
#endif

			std::string shaderVersion = "cs_";
			shaderVersion += desc.ShaderVersion;
			D3D_SHADER_MACRO macros[] = { "HLSL", "1", "COMPUTE", "1", NULL, NULL };
			HRESULT hr = D3DCompileFromFile(ComputeSource.c_str(), macros, D3D_COMPILE_STANDARD_FILE_INCLUDE,
				desc.EntryPoint, shaderVersion.c_str(), flags, 0, &computeShaderBlob, &errorBlob);
			// Should probably do some nicer error handling here #FIXME
			ASSERT(hr == S_OK);

			CS = CD3DX12_SHADER_BYTECODE(computeShaderBlob.Get());
		}
		computeStateDesc.CS = CS;

		computeStateDesc.CachedPSO.pCachedBlob = nullptr;
		computeStateDesc.CachedPSO.CachedBlobSizeInBytes = 0;
		computeStateDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		computeStateDesc.NodeMask = 0;

		HRESULT hr = m_Device->CreateComputePipelineState(&computeStateDesc, IID_PPV_ARGS(&computePipeline_DX12->PipelineState));
		// Should probably do some nicer error handling here #FIXME
		ASSERT(hr == S_OK);

		ComputePipeline computePipeline;
		computePipeline.Desc = desc;
		computePipeline.Data = computePipeline_DX12;

		return computePipeline;
	}

	Texture Device_DX12::CreateTexture(const TextureDesc& desc)
	{
		return CreateTexture(desc, FormatToD3D12Format(desc.Format));
	}
	
	ConstantBuffer Device_DX12::CreateConstantBuffer(const ConstantBufferDesc& desc)
	{
		Ref<ConstantBuffer_DX12> constantBuffer_DX12 = CreateRef<ConstantBuffer_DX12>();
		constantBuffer_DX12->ConstantBufferResource = CreateRef<Resource>();
		constantBuffer_DX12->MemorySize = (desc.Size + 255) & ~255;

		CD3DX12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(constantBuffer_DX12->MemorySize);
		m_Device->CreateCommittedResource(
			&heapProperties,
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&constantBuffer_DX12->ConstantBufferResource->ResourceDX12)
		);

		constantBuffer_DX12->cbv = m_SrvDescHeap.Allocate();

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
		cbvDesc.BufferLocation = constantBuffer_DX12->ConstantBufferResource->ResourceDX12->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = static_cast<u32>(constantBuffer_DX12->MemorySize);
		m_Device->CreateConstantBufferView(&cbvDesc, constantBuffer_DX12->cbv.CPU);

		CD3DX12_RANGE readRange(0, 0);
		constantBuffer_DX12->ConstantBufferResource->ResourceDX12->Map(0, &readRange, &constantBuffer_DX12->GPUAddress);

		constantBuffer_DX12->device = this;

		ConstantBuffer constantBuffer;
		constantBuffer.Desc = desc;
		constantBuffer.Data = constantBuffer_DX12;
		constantBuffer.Buffer = constantBuffer_DX12->GPUAddress;

		return constantBuffer;
	}

	void Device_DX12::UploadTextureData(Texture texture, void* Data, u32 mip, u32 slice)
	{
		Texture_DX12* texture_DX12 = (Texture_DX12*)texture.Data.get();

		D3D12_SUBRESOURCE_DATA subresourceData = {};
		subresourceData.pData = Data;
		subresourceData.RowPitch = static_cast<LONG_PTR>(texture.Desc.Width * FormatSizeInBytes(texture.Desc.Format));
		subresourceData.SlicePitch = static_cast<LONG_PTR>(texture.Desc.Width * texture.Desc.Height * FormatSizeInBytes(texture.Desc.Format));

		u32 subResource = D3D12CalcSubresource(mip, slice, 0, texture.Desc.NumMips, texture.Desc.NumArraySlices);

		if (texture_DX12->TextureResource->CurrentState != D3D12_RESOURCE_STATE_COMMON)
		{

			Ref<CommandBuffer> commandList = GetFreeGraphicsCommandBuffer();

			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				texture_DX12->TextureResource->ResourceDX12.Get(),
				texture_DX12->TextureResource->CurrentState,
				D3D12_RESOURCE_STATE_COMMON,
				subResource
			);

			commandList->CommandList->ResourceBarrier(1, &barrier);
			
			ExecuteGraphicsCommandBuffer(commandList);
			commandList->Wait(m_FenceEvent);
		}

		CopyToResource(
			texture_DX12->TextureResource->ResourceDX12,
			subresourceData,
			subResource
		);

		if (texture_DX12->TextureResource->CurrentState != D3D12_RESOURCE_STATE_COMMON)
		{

			Ref<CommandBuffer> commandList = GetFreeGraphicsCommandBuffer();

			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				texture_DX12->TextureResource->ResourceDX12.Get(),
				D3D12_RESOURCE_STATE_COMMON,
				texture_DX12->TextureResource->CurrentState,
				subResource
			);

			commandList->CommandList->ResourceBarrier(1, &barrier);

			ExecuteGraphicsCommandBuffer(commandList);
			commandList->Wait(m_FenceEvent);
		}
	}

	void Device_DX12::DrawTextureInImGui(Texture texture, u32 width, u32 height)
	{
		Texture_DX12* texture_DX12 = (Texture_DX12*)texture.Data.get();

		ImVec2 size = {
			static_cast<float>(width),
			static_cast<float>(height)
		};
		if (width == 0 || height == 0)
		{
			size = {
				static_cast<float>(texture.Desc.Width),
				static_cast<float>(texture.Desc.Height)
			};
		}

		for (u32 i = 0; i < texture.Desc.NumMips; i++)
		{
			ImGui::Image(static_cast<ImU64>(texture_DX12->mipSrvs[i].GPU.ptr), size);
		}

	};

	void Device_DX12::ImGuiRender(Ref<CommandList> commandList)
	{
		// Execute Command list
		CommandList_DX12& commandListDx12 = *(CommandList_DX12*)commandList.get();

		ImGui::Render();
		ID3D12DescriptorHeap* heap = m_SrvDescHeap.Heap();
		commandListDx12.CommandBuffer->CommandList->SetDescriptorHeaps(1, &heap);
		ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandListDx12.CommandBuffer->CommandList.Get());

		// Update and Render additional Platform Windows
		ImGuiIO& io = ImGui::GetIO(); (void)io;
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault(NULL, (void*)commandListDx12.CommandBuffer->CommandList.Get());
		}

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
	}

#pragma endregion

	// Public Methods

#pragma region Device_DX12 public Methods

	void Device_DX12::SetDeferredReleasesFlag()
	{
		//m_CommandFrames[m_CurrentBackBufferIndex].DeferredReleasesFlag = 1;
	}

	void Device_DX12::Flush()
	{
		for (u32 i = 0; i < m_GraphicsCommandBuffers.size(); i++)
		{
			Ref<CommandBuffer> commandBuffer{ m_GraphicsCommandBuffers[i] };
			commandBuffer->Wait(m_FenceEvent);

			u64 fenceValueForSignal = ++commandBuffer->FenceValue;
			m_GraphicsCommandQueue->Signal(commandBuffer->Fence.Get(), fenceValueForSignal);
			if (commandBuffer->Fence->GetCompletedValue() < commandBuffer->FenceValue)
			{
				commandBuffer->Fence->SetEventOnCompletion(fenceValueForSignal, m_FenceEvent);
				WaitForSingleObject(m_FenceEvent, INFINITE);
			}
		}

		for (u32 i = 0; i < m_ComputeCommandBuffers.size(); i++)
		{
			Ref<CommandBuffer> commandBuffer{ m_ComputeCommandBuffers[i] };
			commandBuffer->Wait(m_FenceEvent);

			u64 fenceValueForSignal = ++commandBuffer->FenceValue;
			m_GraphicsCommandQueue->Signal(commandBuffer->Fence.Get(), fenceValueForSignal);
			if (commandBuffer->Fence->GetCompletedValue() < commandBuffer->FenceValue)
			{
				commandBuffer->Fence->SetEventOnCompletion(fenceValueForSignal, m_FenceEvent);
				WaitForSingleObject(m_FenceEvent, INFINITE);
			}
		}

		for (u32 i = 0; i < m_CopyCommandBuffers.size(); i++)
		{
			Ref<CommandBuffer> commandBuffer{ m_CopyCommandBuffers[i] };
			commandBuffer->Wait(m_FenceEvent);

			u64 fenceValueForSignal = ++commandBuffer->FenceValue;
			m_GraphicsCommandQueue->Signal(commandBuffer->Fence.Get(), fenceValueForSignal);
			if (commandBuffer->Fence->GetCompletedValue() < commandBuffer->FenceValue)
			{
				commandBuffer->Fence->SetEventOnCompletion(fenceValueForSignal, m_FenceEvent);
				WaitForSingleObject(m_FenceEvent, INFINITE);
			}
		}

		//for (u32 i = 0; i < m_GraphicsCommandBuffers.size(); i++)
		//{
		//	m_GraphicsCommandBuffers[i]->Wait(m_FenceEvent);
		//}

		//for (u32 i = 0; i < m_ComputeCommandBuffers.size(); i++)
		//{
		//	m_ComputeCommandBuffers[i]->Wait(m_FenceEvent);
		//}

		//for (u32 i = 0; i < m_CopyCommandBuffers.size(); i++)
		//{
		//	m_CopyCommandBuffers[i]->Wait(m_FenceEvent);
		//}
	}

	Ref<CommandBuffer> Device_DX12::GetFreeGraphicsCommandBuffer()
	{
		u32 index = 0;
		while (m_GraphicsCommandBuffers[index]->IsBusy())
		{
			index = (index + 1) % m_GraphicsCommandBuffers.size();
		}

		DIRECTX12_ASSERT(m_GraphicsCommandBuffers[index]->CommandAllocator->Reset());
		DIRECTX12_ASSERT(m_GraphicsCommandBuffers[index]->CommandList->Reset(m_GraphicsCommandBuffers[index]->CommandAllocator.Get(), nullptr));

		return m_GraphicsCommandBuffers[index];
	}

	Ref<CommandBuffer> Device_DX12::GetFreeComputeCommandBuffer()
	{
		u32 index = 0;
		while (true)
		{
			if (!m_ComputeCommandBuffers[index]->IsBusy())
			{
				DIRECTX12_ASSERT(m_ComputeCommandBuffers[index]->CommandAllocator->Reset());
				DIRECTX12_ASSERT(m_ComputeCommandBuffers[index]->CommandList->Reset(m_ComputeCommandBuffers[index]->CommandAllocator.Get(), nullptr));

				return m_ComputeCommandBuffers[index];
			}
			index++;
			index = index % m_ComputeCommandBuffers.size();
		}
	}

	Ref<CommandBuffer> Device_DX12::GetFreeCopyCommandBuffer()
	{
		u32 index = 0;
		while (true)
		{
			if (!m_CopyCommandBuffers[index]->IsBusy())
			{
				DIRECTX12_ASSERT(m_CopyCommandBuffers[index]->CommandAllocator->Reset());
				DIRECTX12_ASSERT(m_CopyCommandBuffers[index]->CommandList->Reset(m_CopyCommandBuffers[index]->CommandAllocator.Get(), nullptr));

				return m_CopyCommandBuffers[index];
			}
			index++;
			index = index % m_CopyCommandBuffers.size();
		}
	}

	void Device_DX12::ExecuteGraphicsCommandBuffer(Ref<CommandBuffer> graphicsCommandBuffer)
	{
		DIRECTX12_ASSERT(graphicsCommandBuffer->CommandList->Close());
		ID3D12CommandList* const commandLists[]{ graphicsCommandBuffer->CommandList.Get() };
		m_GraphicsCommandQueue->ExecuteCommandLists(_countof(commandLists), &commandLists[0]);

		u64 fenceValueForSignal = ++graphicsCommandBuffer->FenceValue;
		m_GraphicsCommandQueue->Signal(graphicsCommandBuffer->Fence.Get(), fenceValueForSignal);
	}

	void Device_DX12::ExecuteComputeCommandBuffer(Ref<CommandBuffer> computeCommandBuffer)
	{
		DIRECTX12_ASSERT(computeCommandBuffer->CommandList->Close());
		ID3D12CommandList* const commandLists[]{ computeCommandBuffer->CommandList.Get() };
		m_ComputeCommandQueue->ExecuteCommandLists(_countof(commandLists), &commandLists[0]);

		u64 fenceValueForSignal = ++computeCommandBuffer->FenceValue;
		m_ComputeCommandQueue->Signal(computeCommandBuffer->Fence.Get(), fenceValueForSignal);
	}

	void Device_DX12::ExecuteCopyCommandBuffer(Ref<CommandBuffer> copyCommandBuffer)
	{
		DIRECTX12_ASSERT(copyCommandBuffer->CommandList->Close());
		ID3D12CommandList* const commandLists[]{ copyCommandBuffer->CommandList.Get() };
		m_CopyCommandQueue->ExecuteCommandLists(_countof(commandLists), &commandLists[0]);

		u64 fenceValueForSignal = ++copyCommandBuffer->FenceValue;
		m_CopyCommandQueue->Signal(copyCommandBuffer->Fence.Get(), fenceValueForSignal);
	}

	bool Device_DX12::Resize(const Event::EventData& data)
	{
		Flush();
		u32 width = data.Data.u32[0];
		u32 height = data.Data.u32[1];
		LOG_INFO("Resizeing to: %u, %u", width, height);

		if (width == 0) width = 1;
		if (height == 0) height = 1;
		RecreateBackBuffers(width, height);
		return false;
	}

#pragma endregion

	// Private Methods

#pragma region Device_DX12 Private Methods

	void Device_DX12::CreateDevice()
	{
		if (m_Device)
		{
			ASSERT_MSG(false, "Device already exists! cannot create it again!");
		}

		u32 createFactoryFlags = 0;

#ifdef DEBUG
		{
			ComPtr<ID3D12Debug3> debugInterface;
			DIRECTX12_ASSERT(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
			debugInterface->EnableDebugLayer();
			debugInterface->SetEnableGPUBasedValidation(true);
			createFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}
#endif

		DIRECTX12_ASSERT(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&m_Factory)));

		ComPtr<IDXGIAdapter1> adapter = GetAdapter(m_Factory);
		if (!adapter)
		{
			LOG_ERROR("Could not find Adapter!");
			return;
		}
		DXGI_ADAPTER_DESC1 adapterDesc{};
		adapter->GetDesc1(&adapterDesc);
		LOG_DEBUG("Selected Adapter: %ls", adapterDesc.Description);

		D3D_FEATURE_LEVEL maxFeatureLevel{ GetMaxFeatureLevel(adapter) };
		ASSERT(maxFeatureLevel >= c_MinimumFeatureLevel);
		if (maxFeatureLevel < c_MinimumFeatureLevel) return;

		DIRECTX12_ASSERT(D3D12CreateDevice(adapter.Get(), maxFeatureLevel, IID_PPV_ARGS(&m_Device)));

		NAME_DIRECTX12_OBJECT(m_Device, "Main Device");

#ifdef DEBUG
		{
			ComPtr<ID3D12InfoQueue> infoQueue;
			DIRECTX12_ASSERT(m_Device->QueryInterface(IID_PPV_ARGS(&infoQueue)));
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
			infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
		}
#endif
	}

	D3D_FEATURE_LEVEL Device_DX12::GetMaxFeatureLevel(const ComPtr<IDXGIAdapter1>& adapter)
	{
		constexpr D3D_FEATURE_LEVEL c_FeatureLevels[4]
		{
			D3D_FEATURE_LEVEL_11_0,
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_12_0,
			D3D_FEATURE_LEVEL_12_1
		};

		D3D12_FEATURE_DATA_FEATURE_LEVELS featureLevelInfo{};
		featureLevelInfo.NumFeatureLevels = _countof(c_FeatureLevels);
		featureLevelInfo.pFeatureLevelsRequested = c_FeatureLevels;

		ComPtr<ID3D12Device> device;
		DIRECTX12_ASSERT(D3D12CreateDevice(adapter.Get(), c_MinimumFeatureLevel, IID_PPV_ARGS(&device)));
		DIRECTX12_ASSERT(device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &featureLevelInfo, sizeof(featureLevelInfo)));
		return featureLevelInfo.MaxSupportedFeatureLevel;
	}

	ComPtr<IDXGIAdapter4> Device_DX12::GetAdapter(const ComPtr<IDXGIFactory6>& factory)
	{
		// TODO: Select an adapter based on specific requirements
		HRESULT hr{ S_OK };

		ComPtr<IDXGIAdapter4> adapter{ nullptr };

		LOG_DEBUG("Going trough adapters: ");
		for (u32 i = 0; factory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND; i++)
		{
			DXGI_ADAPTER_DESC1 adapterDesc{};
			adapter->GetDesc1(&adapterDesc);
			LOG_DEBUG("\t%ls", adapterDesc.Description);

			hr = D3D12CreateDevice(adapter.Get(), c_MinimumFeatureLevel, __uuidof(ID3D12Device1), nullptr);
			if (SUCCEEDED(hr))
			{
				return adapter;
			}
		}
		
		ASSERT_MSG(false, "Could not find a suiting adapter!");
		return nullptr;
	}

	template<typename T>
	void Device_DX12::CreateCommandBuffers(const ComPtr<ID3D12Device8>& device, 
		ComPtr<ID3D12CommandQueue>* commandQueue, T* commandBuffers, D3D12_COMMAND_LIST_TYPE type)
	{
		D3D12_COMMAND_QUEUE_DESC queueDesc{};
		queueDesc.Type = type;
		queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.NodeMask = 0;

		DIRECTX12_ASSERT(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&(*commandQueue))));

		NAME_DIRECTX12_OBJECT(
			(*commandQueue),
			type == D3D12_COMMAND_LIST_TYPE_DIRECT ? "Graphics Command Queue" :
			type == D3D12_COMMAND_LIST_TYPE_COPY ? "Copy Command Queue" :
			type == D3D12_COMMAND_LIST_TYPE_COMPUTE ? "Compute Command Queue" :
			"Unkown Command Queue"
		);

		for (u32 i = 0; i < (*commandBuffers).size(); i++)
		{
			(*commandBuffers)[i] = CreateRef<CommandBuffer>();
			Ref<CommandBuffer> commandBuffer = (*commandBuffers)[i];

			DIRECTX12_ASSERT(device->CreateCommandAllocator(
				type,
				IID_PPV_ARGS(&commandBuffer->CommandAllocator)
			));

			NAME_DIRECTX12_OBJECT_INDEXED(
				commandBuffer->CommandAllocator,
				i,
				type == D3D12_COMMAND_LIST_TYPE_DIRECT ? "Graphics allocator" :
				type == D3D12_COMMAND_LIST_TYPE_COPY ? "Copy allocator" :
				type == D3D12_COMMAND_LIST_TYPE_COMPUTE ? "Compute allocator" :
				"Unkown allocator"
			);

			DIRECTX12_ASSERT(device->CreateFence(
				0,
				D3D12_FENCE_FLAG_NONE,
				IID_PPV_ARGS(&commandBuffer->Fence)
			));

			NAME_DIRECTX12_OBJECT_INDEXED(
				commandBuffer->Fence, 
				i,
				type == D3D12_COMMAND_LIST_TYPE_DIRECT ? "Graphics Fence" :
				type == D3D12_COMMAND_LIST_TYPE_COPY ? "Copy Fence" :
				type == D3D12_COMMAND_LIST_TYPE_COMPUTE ? "Compute Fence" :
				"Unkown Fence"
			);

			DIRECTX12_ASSERT(device->CreateCommandList(
				0,
				type,
				commandBuffer->CommandAllocator.Get(),
				nullptr,
				IID_PPV_ARGS(&commandBuffer->CommandList)
			));

			DIRECTX12_ASSERT(commandBuffer->CommandList->Close());

			NAME_DIRECTX12_OBJECT_INDEXED(
				commandBuffer->CommandList,
				i,
				type == D3D12_COMMAND_LIST_TYPE_DIRECT ? "Graphics CommandList" :
				type == D3D12_COMMAND_LIST_TYPE_COPY ? "Copy CommandList" :
				type == D3D12_COMMAND_LIST_TYPE_COMPUTE ? "Compute CommandList" :
				"Unkown CommandList"
			);
		}

	}

	Texture Device_DX12::CreateTexture( const TextureDesc& desc, DXGI_FORMAT format, 
		D3D12_RESOURCE_FLAGS flags, D3D12_HEAP_FLAGS heapFlags, const D3D12_CLEAR_VALUE* clearValue)
	{
		Ref<Texture_DX12> texture_DX12 = CreateRef<Texture_DX12>();

		CD3DX12_RESOURCE_DESC textureResourceDesc;

		switch (desc.Type)
		{
		case TextureType::Tex1D:
			textureResourceDesc = CD3DX12_RESOURCE_DESC::Tex1D(
				format,
				desc.Width,
				1,
				static_cast<u16>(desc.NumMips)
			);
			break;
		case TextureType::Tex2D:
			textureResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
				format,
				desc.Width,
				desc.Height,
				1,
				static_cast<u16>(desc.NumMips)
			);
			break;
		case TextureType::Tex3D:
			textureResourceDesc = CD3DX12_RESOURCE_DESC::Tex3D(
				format,
				desc.Width,
				desc.Height,
				static_cast<u16>(desc.Depth),
				static_cast<u16>(desc.NumMips)
			);
			break;
		case TextureType::TexCube:
			textureResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
				format,
				desc.Width,
				desc.Height,
				6,
				static_cast<u16>(desc.NumMips)
			);
			break;
		case TextureType::Tex1DArray:
			textureResourceDesc = CD3DX12_RESOURCE_DESC::Tex1D(
				format,
				desc.Width,
				static_cast<u16>(desc.NumArraySlices),
				static_cast<u16>(desc.NumMips)
			);
			break;
		case TextureType::Tex2DArray:
			textureResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
				format,
				desc.Width,
				desc.Height,
				static_cast<u16>(desc.NumArraySlices),
				static_cast<u16>(desc.NumMips)
			);
			break;
		}

		CD3DX12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

		textureResourceDesc.Flags |= flags;
		if (!(textureResourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL))
		{
			textureResourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		}
		texture_DX12->TextureResource = CreateRef<Resource>();

		texture_DX12->TextureResource->CurrentState = D3D12_RESOURCE_STATE_COMMON;

		DIRECTX12_ASSERT(m_Device->CreateCommittedResource(
			&heapProps,
			heapFlags,
			&textureResourceDesc,
			texture_DX12->TextureResource->CurrentState,
			clearValue,
			IID_PPV_ARGS(&texture_DX12->TextureResource->ResourceDX12)
		));

		// SRV
		{
			texture_DX12->srv = m_SrvDescHeap.Allocate();

			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Format = FormatToD3D12Format(desc.Format);
			srvDesc.ViewDimension = TextureTypeToD3D12SrvDimension(desc.Type);
			switch (desc.Type)
			{
			case TextureType::Tex1D:
				srvDesc.Texture1D.MipLevels = desc.NumMips;
				break;
			case TextureType::Tex2D:
				srvDesc.Texture2D.MipLevels = desc.NumMips;
				break;
			case TextureType::Tex3D:
				srvDesc.Texture3D.MipLevels = desc.NumMips;
				break;
			case TextureType::TexCube:
				srvDesc.TextureCube.MipLevels = desc.NumMips;
				break;
			case TextureType::Tex1DArray:
				srvDesc.Texture1DArray.MipLevels = desc.NumMips;
				srvDesc.Texture1DArray.ArraySize = desc.NumArraySlices;
				break;
			case TextureType::Tex2DArray:
				srvDesc.Texture2DArray.MipLevels = desc.NumMips;
				srvDesc.Texture2DArray.ArraySize = desc.NumArraySlices;
				break;
			}

			m_Device->CreateShaderResourceView(
				texture_DX12->TextureResource->ResourceDX12.Get(),
				&srvDesc,
				texture_DX12->srv.CPU
			);
		}

		// UAV
		if (textureResourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS && desc.Format != Format::R8G8B8A8_SRGB)
		{
			texture_DX12->uav = m_SrvDescHeap.Allocate();

			D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
			uavDesc.Format = FormatToD3D12Format(desc.Format);
			switch (desc.Type)
			{
			case TextureType::Tex1D:
				uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1D;
				uavDesc.Texture1D.MipSlice = 0;
				break;
			case TextureType::Tex2D:
				uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
				uavDesc.Texture2D.MipSlice = 0;
				uavDesc.Texture2D.PlaneSlice = 0;
				break;
			case TextureType::Tex3D:
				uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
				uavDesc.Texture3D.MipSlice = 0;
				uavDesc.Texture3D.FirstWSlice = 0;
				uavDesc.Texture3D.WSize = desc.Depth;
				break;
			case TextureType::TexCube:
				uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
				uavDesc.Texture2DArray.MipSlice = 0;
				uavDesc.Texture2DArray.ArraySize = desc.NumArraySlices;
				uavDesc.Texture2DArray.MipSlice = 0;
				uavDesc.Texture2DArray.PlaneSlice = 0;
				break;
			case TextureType::Tex1DArray:
				uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE1DARRAY;
				uavDesc.Texture1DArray.MipSlice = 0;
				uavDesc.Texture1DArray.ArraySize = desc.NumArraySlices;
				uavDesc.Texture1DArray.MipSlice = 0;
				break;
			case TextureType::Tex2DArray:
				uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
				uavDesc.Texture2DArray.MipSlice = 0;
				uavDesc.Texture2DArray.ArraySize = desc.NumArraySlices;
				uavDesc.Texture2DArray.MipSlice = 0;
				uavDesc.Texture2DArray.PlaneSlice = 0;
				break;
			}

			m_Device->CreateUnorderedAccessView(
				texture_DX12->TextureResource->ResourceDX12.Get(),
				nullptr,
				&uavDesc,
				texture_DX12->uav.CPU
			);
		}

		// Mips
		{
			texture_DX12->mipSrvs.resize(desc.NumMips);
			texture_DX12->mipUavs.resize(desc.NumMips);
			texture_DX12->TextureResource->subResourceStates.resize(desc.NumMips);
			for (u32 i = 0; i < desc.NumMips; i++)
			{
				texture_DX12->mipSrvs[i] = m_SrvDescHeap.Allocate();
				D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
				srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
				srvDesc.Format = FormatToD3D12Format(desc.Format);
				srvDesc.Texture2DArray.MipLevels = 1;
				srvDesc.Texture2DArray.MostDetailedMip = i;
				srvDesc.Texture2DArray.ArraySize = desc.Type == TextureType::TexCube ? 6 : 1;
				srvDesc.Texture2DArray.FirstArraySlice = 0;
				srvDesc.Texture2DArray.PlaneSlice = 0;
				m_Device->CreateShaderResourceView(
					texture_DX12->TextureResource->ResourceDX12.Get(),
					&srvDesc,
					texture_DX12->mipSrvs[i].CPU
				);
				if (textureResourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
				{
					texture_DX12->mipUavs[i] = m_SrvDescHeap.Allocate();
					D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
					uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
					uavDesc.Format = FormatToD3D12Format(desc.Format);
					uavDesc.Format = uavDesc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB ? DXGI_FORMAT_R8G8B8A8_UNORM : uavDesc.Format;
					uavDesc.Texture2DArray.MipSlice = i;
					uavDesc.Texture2DArray.FirstArraySlice = 0;
					uavDesc.Texture2DArray.ArraySize = desc.Type == TextureType::TexCube ? 6 : 1;
					uavDesc.Texture2DArray.PlaneSlice = 0;
					m_Device->CreateUnorderedAccessView(
						texture_DX12->TextureResource->ResourceDX12.Get(),
						nullptr,
						&uavDesc,
						texture_DX12->mipUavs[i].CPU
					);
				}

				texture_DX12->TextureResource->subResourceStates[i] = texture_DX12->TextureResource->CurrentState;
			}

		}

		NAME_DIRECTX12_OBJECT(texture_DX12->TextureResource->ResourceDX12, desc.Name);

		texture_DX12->device = this;

		Texture texture;
		texture.Desc = desc;
		texture.Data = texture_DX12;

		return texture;
	}

	void Device_DX12::CopyToResource(ComPtr<ID3D12Resource>& resource, D3D12_SUBRESOURCE_DATA& subResourceData, u32 subResource)
	{
		ComPtr<ID3D12Resource> intermediateResource;

		UINT64 requiredSize = GetRequiredIntermediateSize(resource.Get(), 0, 1);
		CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC buffer = CD3DX12_RESOURCE_DESC::Buffer(requiredSize);
		m_Device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&buffer,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&intermediateResource)
		);

		Ref<CommandBuffer> copyCommandList = GetFreeCopyCommandBuffer();

		UpdateSubresources(
			copyCommandList->CommandList.Get(),
			resource.Get(),
			intermediateResource.Get(),
			0,
			subResource,
			1,
			&subResourceData
		);

		ExecuteCopyCommandBuffer(copyCommandList);

		copyCommandList->Wait(m_FenceEvent);

	}

	void Device_DX12::RecreateBackBuffers(u32 width, u32 height)
	{
		m_BackBuffers.clear();
		ProcessDeferredReleases();

		DXGI_SWAP_CHAIN_DESC1 desc{};
		m_SwapChain->GetDesc1(&desc);
		m_SwapChain->ResizeBuffers(m_NumBackBuffers, width, height, desc.Format, desc.Flags);

		m_BackBuffers.resize(m_NumBackBuffers);

		for (u32 i = 0; i < m_NumBackBuffers; i++)
		{
			Ref<RenderTarget_DX12> renderTargetDX12 = CreateRef<RenderTarget_DX12>();
			RenderTargetDesc renderTargetDesc;
			renderTargetDesc.RenderTargetFormats[0] = m_BackBufferFormat;
			renderTargetDesc.NumRenderTargets = 1;
			renderTargetDesc.Width = width;
			renderTargetDesc.Height = height;
			renderTargetDesc.RenderTargetClearValues[0].Values[0] = 0.0f;
			renderTargetDesc.RenderTargetClearValues[0].Values[1] = 0.0f;
			renderTargetDesc.RenderTargetClearValues[0].Values[2] = 0.0f;
			renderTargetDesc.RenderTargetClearValues[0].Values[3] = 0.0f;


			D3D12_CLEAR_VALUE clearValue;
			DXGI_FORMAT format = FormatToD3D12Format(renderTargetDesc.RenderTargetFormats[0]);
			clearValue.Format = format;
			clearValue.Color[0] = renderTargetDesc.RenderTargetClearValues[0].Values[0];
			clearValue.Color[1] = renderTargetDesc.RenderTargetClearValues[0].Values[1];
			clearValue.Color[2] = renderTargetDesc.RenderTargetClearValues[0].Values[2];
			clearValue.Color[3] = renderTargetDesc.RenderTargetClearValues[0].Values[3];

			TextureDesc textureDesc;
			textureDesc.Width = renderTargetDesc.Width;
			textureDesc.Height = renderTargetDesc.Height;
			textureDesc.Depth = 1;
			textureDesc.Format = renderTargetDesc.RenderTargetFormats[0];
			textureDesc.NumMips = 1;
			textureDesc.Type = TextureType::Tex2D;

			Ref<Texture_DX12> texture_DX12 = CreateRef<Texture_DX12>();

			texture_DX12->TextureResource = CreateRef<Resource>();
			DIRECTX12_ASSERT(m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&texture_DX12->TextureResource->ResourceDX12)));
			texture_DX12->TextureResource->CurrentState = D3D12_RESOURCE_STATE_COMMON;
			texture_DX12->device = this;

			Texture texture;
			texture.Desc = textureDesc;
			texture.Data = texture_DX12;

			renderTargetDX12->RenderTargetViews[0] = GetRtvHeap().Allocate();

			D3D12_RENDER_TARGET_VIEW_DESC renderTargetViewDesc{};
			renderTargetViewDesc.Format = format;
			renderTargetViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			m_Device->CreateRenderTargetView(texture_DX12->TextureResource->ResourceDX12.Get(), &renderTargetViewDesc, renderTargetDX12->RenderTargetViews[0].CPU);
			NAME_DIRECTX12_OBJECT_INDEXED(texture_DX12->TextureResource->ResourceDX12, i, "BackBuffer");

			renderTargetDX12->rect.left = 0;
			renderTargetDX12->rect.top = 0;
			renderTargetDX12->rect.right = static_cast<i32>(width);
			renderTargetDX12->rect.bottom = static_cast<i32>(height);

			renderTargetDX12->ViewPort.TopLeftX = 0.f;
			renderTargetDX12->ViewPort.TopLeftY = 0.f;
			renderTargetDX12->ViewPort.Width = static_cast<f32>(width);
			renderTargetDX12->ViewPort.Height = static_cast<f32>(height);
			renderTargetDX12->ViewPort.MinDepth = 0.f;
			renderTargetDX12->ViewPort.MaxDepth = 1.f;

			renderTargetDX12->device = this;

			m_BackBuffers[i].Desc = renderTargetDesc;
			m_BackBuffers[i].Data = renderTargetDX12;
			m_BackBuffers[i].RenderTextures[0] = texture;
		}

		m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();
	}

	void __declspec(noinline) Device_DX12::ProcessDeferredReleases()
	{
		std::lock_guard<std::mutex> lock(m_DeferredReleasesMutex);

		m_RtvDescHeap.ProcessDeferredFree();
		m_DsvDescHeap.ProcessDeferredFree();
		m_SrvDescHeap.ProcessDeferredFree();
		m_UavDescHeap.ProcessDeferredFree();
	}

#pragma endregion

} }

#endif // WIN32

