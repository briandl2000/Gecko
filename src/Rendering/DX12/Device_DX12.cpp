#ifdef DIRECTX_12

#include "Rendering/DX12/Device_DX12.h"
#include "Core/Platform.h"
#include "CommandList_DX12.h"

#include "Objects_DX12.h"

#include <dxgi1_6.h>
#include "d3dx12.h"
#include <d3dcompiler.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx12.h>
#include "Device_DX12.h"

#define SizeOfInUint32(obj) ((sizeof(obj) - 1) / sizeof(UINT32) + 1)
#if defined(min)
#undef min
#endif
#if defined(max)
#undef max
#endif

namespace Gecko::DX12
{

	// Device Interface Methods

	static Device_DX12* s_Device{ nullptr };

#pragma region Device Interface Methods

	void Device_DX12::Init()
	{
		if (s_Device != nullptr)
		{
			ASSERT(false, "Device already initialized!")
		}
		s_Device = this;

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
		ASSERT(m_FenceEvent, "Fence creation failed");

		// Create Discriptor heaps
		{
			bool result = true;
			result &= m_RtvDescHeap.Initialize(this, 50, false);
			result &= m_DsvDescHeap.Initialize(this, 512, false);
			result &= m_SrvDescHeap.Initialize(this, 4096, true);
			ASSERT(result, "Failed to initialize heaps!");
		}

		// Creating the swap chain
		DXGI_SWAP_CHAIN_DESC1 swapchainDesc{};
		m_BackBufferFormat = DataFormat::R8G8B8A8_UNORM;
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

		Flush();

		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();

		CloseHandle(m_FenceEvent);
		m_NumBackBuffers = 0;

		m_BackBuffers.clear();

		ProcessDeferredReleases();

		m_RtvDescHeap.Destroy();
		m_DsvDescHeap.Destroy();
		m_SrvDescHeap.Destroy();

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

#if defined(DX12_DEBUG_LAYER)
		{
			ComPtr<IDXGIDebug1> debugDevice;
			DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debugDevice));
			debugDevice->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
		}
#endif

		RemoveEventListener(Event::SystemEvent::CODE_RESIZED, &Device_DX12::Resize);

		ASSERT(s_Device != nullptr, "Device is already shut down!");
		s_Device = nullptr;
	}

	Ref<CommandList> Device_DX12::CreateGraphicsCommandList()
	{
		Ref<CommandBuffer> graphicsCommandBuffer = GetFreeGraphicsCommandBuffer();
		Ref<CommandList_DX12> commandList = CreateRef<CommandList_DX12>();
		commandList->CommandBuffer = graphicsCommandBuffer;

		ID3D12DescriptorHeap* descriptorHeaps[] = { m_SrvDescHeap.Heap() };
		commandList->CommandBuffer->CommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

		return commandList;
	}

	void Device_DX12::ExecuteGraphicsCommandList(Ref<CommandList> commandList)
	{
		ASSERT(commandList->IsValid(), "Command list is invalid!");
		CommandList_DX12* commandList_DX12 = static_cast<CommandList_DX12*>(commandList.get());

		DIRECTX12_ASSERT(commandList_DX12->CommandBuffer->CommandList->Close());
		ID3D12CommandList* const commandLists[]{ commandList_DX12->CommandBuffer->CommandList.Get() };
		m_GraphicsCommandQueue->ExecuteCommandLists(_countof(commandLists), &commandLists[0]);

		u64 fenceValueForSignal = ++commandList_DX12->CommandBuffer->FenceValue;
		m_GraphicsCommandQueue->Signal(commandList_DX12->CommandBuffer->Fence.Get(), fenceValueForSignal);
		commandList_DX12->CommandBuffer.reset();
	}

	void Device_DX12::ExecuteGraphicsCommandListAndFlip(Ref<CommandList> commandList)
	{
		ASSERT(commandList->IsValid(), "Command list is invalid!");
		CommandList_DX12* commandList_DX12 = static_cast<CommandList_DX12*>(commandList.get());
		commandList_DX12->TransitionRenderTarget(GetCurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COMMON);

		ExecuteGraphicsCommandList(commandList);

		ASSERT(m_SwapChain, "Swap chain doesn't exist!");
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
		ASSERT(commandList->IsValid(), "Command list is invalid!");
		CommandList_DX12* commandList_DX12 = static_cast<CommandList_DX12*>(commandList.get());

		DIRECTX12_ASSERT(commandList_DX12->CommandBuffer->CommandList->Close());
		ID3D12CommandList* const commandLists[]{ commandList_DX12->CommandBuffer->CommandList.Get() };
		m_ComputeCommandQueue->ExecuteCommandLists(_countof(commandLists), &commandLists[0]);

		u64 fenceValueForSignal = ++commandList_DX12->CommandBuffer->FenceValue;
		m_ComputeCommandQueue->Signal(commandList_DX12->CommandBuffer->Fence.Get(), fenceValueForSignal);
		commandList_DX12->CommandBuffer = nullptr;
	}

	RenderTarget Device_DX12::CreateRenderTarget(const RenderTargetDesc& desc)
	{
		{ // Validity checks
			std::string failureReason{};
			ASSERT(desc.IsValid(&failureReason), failureReason.c_str());
		}

		Ref<RenderTarget_DX12> renderTarget_DX12 = CreateRef<RenderTarget_DX12>();
		RenderTarget renderTarget(desc);

		for (u32 i = 0; i < desc.NumRenderTargets; i++)
		{
			DXGI_FORMAT format = FormatToD3D12Format(desc.RenderTargetFormats[i]);

			D3D12_CLEAR_VALUE clearValue;
			clearValue.Format = format;
			clearValue.Color[0] = desc.RenderTargetClearValues[i].Values[0];
			clearValue.Color[1] = desc.RenderTargetClearValues[i].Values[1];
			clearValue.Color[2] = desc.RenderTargetClearValues[i].Values[2];
			clearValue.Color[3] = desc.RenderTargetClearValues[i].Values[3];

			u32 numTextureMips = std::min(desc.NumMips[i], CalculateNumberOfMips(desc.Width, desc.Height));
			TextureDesc textureDesc;
			textureDesc.Width = desc.Width;
			textureDesc.Height = desc.Height;
			textureDesc.Depth = 1;
			textureDesc.Format = desc.RenderTargetFormats[i];
			textureDesc.NumMips = numTextureMips;
			textureDesc.Type = TextureType::Tex2D;

			renderTarget.RenderTextures[i] = CreateTexture(textureDesc, FormatToD3D12Format(desc.RenderTargetFormats[i]),
				D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES,
				&clearValue);

			Texture_DX12* texture_DX12 = reinterpret_cast<Texture_DX12*>(renderTarget.RenderTextures[i].Data.get());

			renderTarget_DX12->RenderTargetViews[i] = GetRtvHeap().Allocate();

			D3D12_RENDER_TARGET_VIEW_DESC renderTargetDesc{};
			renderTargetDesc.Format = format;
			renderTargetDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			m_Device->CreateRenderTargetView(texture_DX12->TextureResource->ResourceDX12.Get(), &renderTargetDesc, renderTarget_DX12->RenderTargetViews[i].CPU);
		}

		if (desc.DepthStencilFormat != DataFormat::None)
		{
			D3D12_CLEAR_VALUE clearValue;
			clearValue.Format = DXGI_FORMAT_D32_FLOAT;
			clearValue.DepthStencil.Depth = desc.DepthTargetClearValue.Depth;
			clearValue.DepthStencil.Stencil = 0;

			u32 numTextureMips = std::min(desc.DepthMips, CalculateNumberOfMips(desc.Width, desc.Height));
			TextureDesc textureDesc;
			textureDesc.Width = desc.Width;
			textureDesc.Height = desc.Height;
			textureDesc.Depth = 1;
			textureDesc.Format = desc.DepthStencilFormat;
			textureDesc.NumMips = numTextureMips;
			textureDesc.Type = TextureType::Tex2D;

			renderTarget.DepthTexture = CreateTexture(textureDesc,
				DXGI_FORMAT_D32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL, D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES,
				&clearValue);

			Texture_DX12* texture_DX12 = reinterpret_cast<Texture_DX12*>(renderTarget.DepthTexture.Data.get());

			renderTarget_DX12->DepthStencilView = GetDsvHeap().Allocate();

			D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc;
			depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
			depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			depthStencilDesc.Texture2D.MipSlice = 0;
			depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;
			m_Device->CreateDepthStencilView(texture_DX12->TextureResource->ResourceDX12.Get(), &depthStencilDesc, renderTarget_DX12->DepthStencilView.CPU);
		}


		renderTarget_DX12->Rect.left = 0;
		renderTarget_DX12->Rect.top = 0;
		renderTarget_DX12->Rect.right = static_cast<i32>(desc.Width);
		renderTarget_DX12->Rect.bottom = static_cast<i32>(desc.Height);

		renderTarget_DX12->ViewPort.TopLeftX = 0.f;
		renderTarget_DX12->ViewPort.TopLeftY = 0.f;
		renderTarget_DX12->ViewPort.Width = static_cast<float>(desc.Width);
		renderTarget_DX12->ViewPort.Height = static_cast<float>(desc.Height);
		renderTarget_DX12->ViewPort.MinDepth = 0.f;
		renderTarget_DX12->ViewPort.MaxDepth = 1.f;

		renderTarget.Data = renderTarget_DX12;

		return renderTarget;
	}

	Buffer Device_DX12::CreateVertexBuffer(const VertexBufferDesc& desc)
	{
		{ // Validity checks
			std::string failureReason{};
			ASSERT(desc.IsValid(&failureReason), failureReason.c_str());
		}

		BufferDesc bufferDesc{ };
		bufferDesc.Type = BufferType::Vertex;
		bufferDesc.NumElements = desc.NumVertices;
		bufferDesc.Stride = desc.Layout.Stride;
		bufferDesc.MemoryType = desc.MemoryType;
		bufferDesc.CanReadWrite = desc.CanReadWrite;

		Ref<Buffer_DX12> buffer_DX12 = CreateBuffer(bufferDesc);

		buffer_DX12->VertexBufferView.BufferLocation = buffer_DX12->BufferResource->ResourceDX12->GetGPUVirtualAddress();
		buffer_DX12->VertexBufferView.SizeInBytes = static_cast<u32>(buffer_DX12->AllocatedMemorySize);
		buffer_DX12->VertexBufferView.StrideInBytes = bufferDesc.Stride;

		Buffer buffer(bufferDesc);
		buffer.Data = buffer_DX12;

		return buffer;
	}

	Buffer Device_DX12::CreateIndexBuffer(const IndexBufferDesc& desc)
	{
		{ // Validity checks
			std::string failureReason{};
			ASSERT(desc.IsValid(&failureReason), failureReason.c_str());
		}

		BufferDesc bufferDesc{ };
		bufferDesc.Type = BufferType::Index;
		bufferDesc.NumElements = desc.NumIndices;
		bufferDesc.Stride = FormatSizeInBytes(desc.IndexFormat);
		bufferDesc.MemoryType = desc.MemoryType;
		bufferDesc.CanReadWrite = desc.CanReadWrite;

		Ref<Buffer_DX12> buffer_DX12 = CreateBuffer(bufferDesc);

		buffer_DX12->IndexBufferView.BufferLocation = buffer_DX12->BufferResource->ResourceDX12->GetGPUVirtualAddress();
		buffer_DX12->IndexBufferView.SizeInBytes = static_cast<u32>(buffer_DX12->AllocatedMemorySize);
		buffer_DX12->IndexBufferView.Format = FormatToD3D12Format(desc.IndexFormat);

		Buffer buffer(bufferDesc);
		buffer.Data = buffer_DX12;

		return buffer;
	}

	Buffer Device_DX12::CreateConstantBuffer(const ConstantBufferDesc& desc)
	{
		{ // Validity checks
			std::string failureReason{};
			ASSERT(desc.IsValid(&failureReason), failureReason.c_str());
		}

		BufferDesc bufferDesc{ };
		bufferDesc.Type = BufferType::Constant;
		bufferDesc.Size = desc.Size;
		bufferDesc.MemoryType = desc.MemoryType;
		bufferDesc.CanReadWrite = false;

		Ref<Buffer_DX12> buffer_DX12 = CreateBuffer(bufferDesc);

		Buffer buffer(bufferDesc);
		buffer.Data = buffer_DX12;

		return buffer;
	}

	Buffer Device_DX12::CreateStructuredBuffer(const StructuredBufferDesc& desc)
	{
		{ // Validity checks
			std::string failureReason{};
			ASSERT(desc.IsValid(&failureReason), failureReason.c_str());
		}

		BufferDesc bufferDesc{ };
		bufferDesc.Type = BufferType::Structured;
		bufferDesc.Stride = desc.StructSize;
		bufferDesc.NumElements = desc.NumElements;
		bufferDesc.MemoryType = desc.MemoryType;
		bufferDesc.CanReadWrite = desc.CanReadWrite;

		Ref<Buffer_DX12> buffer_DX12 = CreateBuffer(bufferDesc);

		Buffer buffer(bufferDesc);
		buffer.Data = buffer_DX12;

		return buffer;
	}

	Texture Device_DX12::CreateTexture(const TextureDesc& desc)
	{
		{ // Validity checks
			std::string failureReason{};
			ASSERT(desc.IsValid(&failureReason), failureReason.c_str());
		}
		return CreateTexture(desc, FormatToD3D12Format(desc.Format));
	}

	GraphicsPipeline Device_DX12::CreateGraphicsPipeline(const GraphicsPipelineDesc& desc)
	{
		{ // Validity checks
			std::string failureReason{};
			ASSERT(desc.IsValid(&failureReason), failureReason.c_str());
		}

		Ref<GraphicsPipeline_DX12> graphicsPipeline_DX12 = CreateRef<GraphicsPipeline_DX12>();

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

			u32 numRootParams = static_cast<u32>(desc.PipelineResources.size());
			rootParameters.resize(numRootParams);
			u32 numDescriptorTableRanges = numRootParams;
			descriptorTableRanges.resize(numDescriptorTableRanges);

			for (u32 i = 0; i < numDescriptorTableRanges; i++)
			{
				u32 descriptorIndex = i;
				const PipelineResource& pipelineResource = desc.PipelineResources[i];

				switch (pipelineResource.Type)
				{
				case ResourceType::None: ASSERT(false, "None is not a valid resource type"); continue;
				case ResourceType::Texture:
				{
					descriptorTableRanges[descriptorIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
					descriptorTableRanges[descriptorIndex].NumDescriptors = 1;
					descriptorTableRanges[descriptorIndex].BaseShaderRegister = pipelineResource.BindLocation;
					descriptorTableRanges[descriptorIndex].RegisterSpace = 0;
					descriptorTableRanges[descriptorIndex].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

					rootParameters[descriptorIndex].InitAsDescriptorTable(1, &descriptorTableRanges[descriptorIndex],
						ShaderTypeToD3D12ShaderVisibility(pipelineResource.Visibility));
					graphicsPipeline_DX12->TextureIndices.push_back(descriptorIndex);
				}
				continue;
				case ResourceType::ConstantBuffer:
				{
					descriptorTableRanges[descriptorIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
					descriptorTableRanges[descriptorIndex].NumDescriptors = 1;
					descriptorTableRanges[descriptorIndex].BaseShaderRegister = pipelineResource.BindLocation;
					descriptorTableRanges[descriptorIndex].RegisterSpace = 0;
					descriptorTableRanges[descriptorIndex].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

					rootParameters[descriptorIndex].InitAsDescriptorTable(1, &descriptorTableRanges[descriptorIndex],
						ShaderTypeToD3D12ShaderVisibility(pipelineResource.Visibility));
					graphicsPipeline_DX12->ConstantBufferIndices.push_back(descriptorIndex);
				}
				continue;
				case ResourceType::StructuredBuffer:
				{
					descriptorTableRanges[descriptorIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
					descriptorTableRanges[descriptorIndex].NumDescriptors = 1;
					descriptorTableRanges[descriptorIndex].BaseShaderRegister = pipelineResource.BindLocation;
					descriptorTableRanges[descriptorIndex].RegisterSpace = 0;
					descriptorTableRanges[descriptorIndex].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

					rootParameters[descriptorIndex].InitAsDescriptorTable(1, &descriptorTableRanges[descriptorIndex],
						ShaderTypeToD3D12ShaderVisibility(pipelineResource.Visibility));
					graphicsPipeline_DX12->TextureIndices.push_back(descriptorIndex);
				}
				continue;
				case ResourceType::LocalData:
				{
					rootParameters[descriptorIndex].InitAsConstants(
						pipelineResource.Size / 4,
						pipelineResource.BindLocation,
						0,
						ShaderTypeToD3D12ShaderVisibility(pipelineResource.Visibility)
					);
					graphicsPipeline_DX12->LocalDataLocation = static_cast<u32>(graphicsPipeline_DX12->ConstantBufferIndices.size());
					graphicsPipeline_DX12->ConstantBufferIndices.push_back(descriptorIndex);
				}
				continue;
				}
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
				elementDesc.Format = FormatToD3D12Format(vertexAttrib.AttributeFormat);
				elementDesc.InputSlot = 0;
				elementDesc.AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
				elementDesc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
				elementDesc.InstanceDataStepRate = 0;
				inputLayoutElements.push_back(elementDesc);
			}
			InputLayout = { inputLayoutElements.data(), static_cast<u32>(inputLayoutElements.size()) };
		}
		piplineStateStream2.InputLayout = InputLayout;

		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY primitiveTopologyType;
		{
			primitiveTopologyType = PrimitiveTypeToD3D12PrimitiveTopologyType(desc.PrimitiveType);
		}
		piplineStateStream2.PrimitiveTopologyType = primitiveTopologyType;

		CD3DX12_PIPELINE_STATE_STREAM_VS VS;
		ComPtr<ID3DBlob> vertexShaderBlob;
		if (desc.VertexShaderPath != nullptr)
		{
			std::string vertexPath = Platform::GetLocalPath(desc.VertexShaderPath);
			std::wstring vertexSource;
			vertexSource.assign(vertexPath.begin(), vertexPath.end());

			ComPtr<ID3DBlob> errorBlob;
			UINT flags = D3DCOMPILE_ENABLE_STRICTNESS;
#if defined( DX12_DEBUG_LAYER )
			flags |= D3DCOMPILE_DEBUG;
#endif

			std::string shaderVersion = "vs_";
			shaderVersion += desc.ShaderVersion;
			D3D_SHADER_MACRO macros[] = { "HLSL", "1", "VERTEX", "1", NULL, NULL };
			HRESULT hr = D3DCompileFromFile(vertexSource.c_str(), macros, D3D_COMPILE_STANDARD_FILE_INCLUDE,
				desc.VertexEntryPoint, shaderVersion.c_str(), flags, 0, &vertexShaderBlob, &errorBlob);
			ASSERT(hr == S_OK, reinterpret_cast<char*>(errorBlob->GetBufferPointer()));

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
#if defined( DX12_DEBUG_LAYER )
			flags |= D3DCOMPILE_DEBUG;
#endif

			std::string shaderVersion = "ps_";
			shaderVersion += desc.ShaderVersion;
			D3D_SHADER_MACRO macros[] = { "HLSL", "1", "PIXEL", "1", NULL, NULL };
			HRESULT hr = D3DCompileFromFile(pixelSource.c_str(), macros, D3D_COMPILE_STANDARD_FILE_INCLUDE,
				desc.PixelEntryPoint, shaderVersion.c_str(), flags, 0, &pixelShaderBlob, &errorBlob);
			ASSERT(hr == S_OK, reinterpret_cast<char*>(errorBlob->GetBufferPointer()));

			PS = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
			piplineStateStream2.PS = PS;
		}

		D3D12_RT_FORMAT_ARRAY rtvFormats = { {DXGI_FORMAT_UNKNOWN}, 1 };
		rtvFormats.NumRenderTargets = desc.NumRenderTargets;
		{
			for (u32 i = 0; i < desc.NumRenderTargets; i++)
			{
				ASSERT(desc.RenderTextureFormats[i] != DataFormat::None, "None is invalid for rendertarget format!");
				rtvFormats.RTFormats[i] = FormatToD3D12Format(desc.RenderTextureFormats[i]);
			}
			if (rtvFormats.NumRenderTargets > 0)
			{
				piplineStateStream2.RTVFormats = rtvFormats;
			}
		}

		CD3DX12_DEPTH_STENCIL_DESC1 DepthStencilState;
		{
			CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormats;
			if (desc.DepthStencilFormat != DataFormat::None)
			{
				DSVFormats = DXGI_FORMAT_D32_FLOAT;
				DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC1(CD3DX12_DEFAULT());
				DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
			}
			else
			{
				DSVFormats = DXGI_FORMAT_UNKNOWN;
				DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC1();
				DepthStencilState.DepthEnable = false;
			}
			piplineStateStream2.DSVFormat = DSVFormats;
			piplineStateStream2.DepthStencilState = DepthStencilState;
		}

		CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
		{
			Rasterizer = CD3DX12_RASTERIZER_DESC(
				D3D12_FILL_MODE_SOLID, // PrimitiveTypeToD3D12FillMode(desc.PrimitiveType),
				CullModeToD3D12CullMode(desc.CullMode),
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

		DIRECTX12_ASSERT_MSG(m_Device->CreateGraphicsPipelineState(&graphicsDesc,
			IID_PPV_ARGS(&graphicsPipeline_DX12->PipelineState)), "Pipeline creation failed, did you fill in the descriptor correctly?");

		GraphicsPipeline graphicsPipeline(desc);
		graphicsPipeline.Data = graphicsPipeline_DX12;

		return graphicsPipeline;
	}

	ComputePipeline Device_DX12::CreateComputePipeline(const ComputePipelineDesc& desc)
	{
		{ // Validity checks
			std::string failureReason{};
			ASSERT(desc.IsValid(&failureReason), failureReason.c_str());
		}
		Ref<ComputePipeline_DX12> computePipeline_DX12 = CreateRef<ComputePipeline_DX12>();

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

			u32 numRootParams = static_cast<u32>(desc.PipelineReadOnlyResources.size()) + static_cast<u32>(desc.PipelineReadWriteResources.size());
			rootParameters.resize(numRootParams);
			u32 numDescriptorTableRanges = numRootParams;
			descriptorTableRanges.resize(numDescriptorTableRanges);

			for (u32 i = 0; i < desc.PipelineReadOnlyResources.size(); i++)
			{
				u32 descriptorIndex = i;
				const PipelineResource& pipelineResource = desc.PipelineReadOnlyResources[i];

				switch (pipelineResource.Type)
				{
				case ResourceType::None: ASSERT(false, "None is not a valid resource type"); continue;
				case ResourceType::Texture:
				{
					descriptorTableRanges[descriptorIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
					descriptorTableRanges[descriptorIndex].NumDescriptors = 1;
					descriptorTableRanges[descriptorIndex].BaseShaderRegister = pipelineResource.BindLocation;
					descriptorTableRanges[descriptorIndex].RegisterSpace = 0;
					descriptorTableRanges[descriptorIndex].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

					rootParameters[descriptorIndex].InitAsDescriptorTable(1, &descriptorTableRanges[descriptorIndex],
						ShaderTypeToD3D12ShaderVisibility(pipelineResource.Visibility));
					computePipeline_DX12->TextureIndices.push_back(descriptorIndex);
				}
				continue;
				case ResourceType::ConstantBuffer:
				{
					descriptorTableRanges[descriptorIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
					descriptorTableRanges[descriptorIndex].NumDescriptors = 1;
					descriptorTableRanges[descriptorIndex].BaseShaderRegister = pipelineResource.BindLocation;
					descriptorTableRanges[descriptorIndex].RegisterSpace = 0;
					descriptorTableRanges[descriptorIndex].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

					rootParameters[descriptorIndex].InitAsDescriptorTable(1, &descriptorTableRanges[descriptorIndex],
						ShaderTypeToD3D12ShaderVisibility(pipelineResource.Visibility));
					computePipeline_DX12->ConstantBufferIndices.push_back(descriptorIndex);
				}
				continue;
				case ResourceType::StructuredBuffer:
				{
					descriptorTableRanges[descriptorIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
					descriptorTableRanges[descriptorIndex].NumDescriptors = 1;
					descriptorTableRanges[descriptorIndex].BaseShaderRegister = pipelineResource.BindLocation;
					descriptorTableRanges[descriptorIndex].RegisterSpace = 0;
					descriptorTableRanges[descriptorIndex].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

					rootParameters[descriptorIndex].InitAsDescriptorTable(1, &descriptorTableRanges[descriptorIndex],
						ShaderTypeToD3D12ShaderVisibility(pipelineResource.Visibility));
					computePipeline_DX12->TextureIndices.push_back(descriptorIndex);
				}
				continue;
				case ResourceType::LocalData:
				{
					rootParameters[descriptorIndex].InitAsConstants(
						pipelineResource.Size / 4,
						pipelineResource.BindLocation,
						0,
						ShaderTypeToD3D12ShaderVisibility(pipelineResource.Visibility)
					);
					computePipeline_DX12->LocalDataLocation = i;
					computePipeline_DX12->ConstantBufferIndices.push_back(descriptorIndex);
				}
				continue;
				}
			}
			u32 uavOffset = static_cast<u32>(desc.PipelineReadOnlyResources.size());

			for (u32 i = 0; i < desc.PipelineReadWriteResources.size(); i++)
			{
				u32 descriptorIndex = i + uavOffset;
				const PipelineResource& pipelineResource = desc.PipelineReadWriteResources[i];

				descriptorTableRanges[descriptorIndex].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
				descriptorTableRanges[descriptorIndex].NumDescriptors = 1;
				descriptorTableRanges[descriptorIndex].BaseShaderRegister = pipelineResource.BindLocation;
				descriptorTableRanges[descriptorIndex].RegisterSpace = 0;
				descriptorTableRanges[descriptorIndex].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

				rootParameters[descriptorIndex].InitAsDescriptorTable(1, &descriptorTableRanges[descriptorIndex],
					ShaderTypeToD3D12ShaderVisibility(pipelineResource.Visibility));
				computePipeline_DX12->ReadWriteIndices.push_back(descriptorIndex);
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
#if defined( DX12_DEBUG_LAYER )
			flags |= D3DCOMPILE_DEBUG;
#endif

			std::string shaderVersion = "cs_";
			shaderVersion += desc.ShaderVersion;
			D3D_SHADER_MACRO macros[] = { "HLSL", "1", "COMPUTE", "1", NULL, NULL };
			HRESULT hr = D3DCompileFromFile(ComputeSource.c_str(), macros, D3D_COMPILE_STANDARD_FILE_INCLUDE,
				desc.EntryPoint, shaderVersion.c_str(), flags, 0, &computeShaderBlob, &errorBlob);
			// Should probably do some nicer error handling here #FIXME
			ASSERT(hr == S_OK, reinterpret_cast<char*>(errorBlob->GetBufferPointer()));

			CS = CD3DX12_SHADER_BYTECODE(computeShaderBlob.Get());
		}
		computeStateDesc.CS = CS;

		computeStateDesc.CachedPSO.pCachedBlob = nullptr;
		computeStateDesc.CachedPSO.CachedBlobSizeInBytes = 0;
		computeStateDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		computeStateDesc.NodeMask = 0;

		HRESULT hr = m_Device->CreateComputePipelineState(&computeStateDesc, IID_PPV_ARGS(&computePipeline_DX12->PipelineState));
		// Should probably do some nicer error handling here #FIXME
		DIRECTX12_ASSERT_MSG(hr, "Failed to create compute pipeline!");

		ComputePipeline computePipeline(desc);
		computePipeline.Data = computePipeline_DX12;

		return computePipeline;
	}

	void Device_DX12::UploadTextureData(Texture texture, void* data, u32 mip, u32 slice)
	{
		{ // Validity checks
			std::string failureReason{};
			ASSERT(texture.IsValid(&failureReason), failureReason.c_str());
			ASSERT(data != nullptr, "Data cannot be nullptr");
		}
		Texture_DX12* texture_DX12 = (Texture_DX12*)texture.Data.get();

		D3D12_SUBRESOURCE_DATA subresourceData = {};
		subresourceData.pData = data;
		subresourceData.RowPitch = static_cast<LONG_PTR>(texture.Desc.Width * FormatSizeInBytes(texture.Desc.Format));
		subresourceData.SlicePitch = static_cast<LONG_PTR>(texture.Desc.Width * texture.Desc.Height * FormatSizeInBytes(texture.Desc.Format));

		u32 subResource = D3D12CalcSubresource(mip, slice, 0, texture.Desc.NumMips, texture.Desc.NumArraySlices);

		CopyToResource(texture_DX12->TextureResource->ResourceDX12, subresourceData, subResource);
	}

	void Device_DX12::UploadBufferData(Buffer buffer, void* data, u32 size, u32 offset)
	{
		{ // Validity checks
			std::string failureReason{};
			ASSERT(buffer.IsValid(&failureReason), failureReason.c_str());
			ASSERT(size > 0, "Size must be bigger than 0!");
			ASSERT(data != nullptr, "Data cannot be nullptr");
		}

		Buffer_DX12* buffer_DX12 = reinterpret_cast<Buffer_DX12*>(buffer.Data.get());

		if (buffer.Desc.MemoryType == MemoryType::Shared)
		{
			CD3DX12_RANGE readRange(0, 0);
			u8* cpyLocation = nullptr;
			buffer_DX12->BufferResource->ResourceDX12->Map(0, &readRange, reinterpret_cast<void**>(&cpyLocation));
			cpyLocation += offset;
			memcpy(cpyLocation, data, size);
			buffer_DX12->BufferResource->ResourceDX12->Unmap(0, &readRange);
		}
		else
		{
			D3D12_SUBRESOURCE_DATA subresourceData = {};
			subresourceData.pData = data;
			subresourceData.RowPitch = size;
			subresourceData.SlicePitch = subresourceData.RowPitch;

			CopyToResource(buffer_DX12->BufferResource->ResourceDX12, subresourceData, 0, offset);
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
			ImGui::Image(static_cast<ImU64>(texture_DX12->MipShaderResourceViews[i].GPU.ptr), size);
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

	void Device_DX12::Flush()
	{
		for (u32 i = 0; i < m_GraphicsCommandBuffers.size(); i++)
		{
			Ref<CommandBuffer> commandBuffer{ m_GraphicsCommandBuffers[i] };
			//commandBuffer->Wait(m_FenceEvent);

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
			//commandBuffer->Wait(m_FenceEvent);

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
			//commandBuffer->Wait(m_FenceEvent);

			u64 fenceValueForSignal = ++commandBuffer->FenceValue;
			m_GraphicsCommandQueue->Signal(commandBuffer->Fence.Get(), fenceValueForSignal);
			if (commandBuffer->Fence->GetCompletedValue() < commandBuffer->FenceValue)
			{
				commandBuffer->Fence->SetEventOnCompletion(fenceValueForSignal, m_FenceEvent);
				WaitForSingleObject(m_FenceEvent, INFINITE);
			}
		}
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
		while (m_ComputeCommandBuffers[index]->IsBusy())
		{
			index = (index + 1) % m_ComputeCommandBuffers.size();
		}

		DIRECTX12_ASSERT(m_ComputeCommandBuffers[index]->CommandAllocator->Reset());
		DIRECTX12_ASSERT(m_ComputeCommandBuffers[index]->CommandList->Reset(m_ComputeCommandBuffers[index]->CommandAllocator.Get(), nullptr));

		return m_ComputeCommandBuffers[index];
	}

	Ref<CommandBuffer> Device_DX12::GetFreeCopyCommandBuffer()
	{
		u32 index = 0;
		while (m_CopyCommandBuffers[index]->IsBusy())
		{
			index = (index + 1) % m_CopyCommandBuffers.size();
		}

		DIRECTX12_ASSERT(m_CopyCommandBuffers[index]->CommandAllocator->Reset());
		DIRECTX12_ASSERT(m_CopyCommandBuffers[index]->CommandList->Reset(m_CopyCommandBuffers[index]->CommandAllocator.Get(), nullptr));

		return m_CopyCommandBuffers[index];
	}

	void Device_DX12::ExecuteGraphicsCommandBuffer(Ref<CommandBuffer> graphicsCommandBuffer)
	{
		DIRECTX12_ASSERT(graphicsCommandBuffer->CommandList->Close());
		ID3D12CommandList* const commandLists[]{ graphicsCommandBuffer->CommandList.Get() };
		m_GraphicsCommandQueue->ExecuteCommandLists(_countof(commandLists), &commandLists[0]);

		u64 fenceValueForSignal = ++graphicsCommandBuffer->FenceValue;
		DIRECTX12_ASSERT(m_GraphicsCommandQueue->Signal(graphicsCommandBuffer->Fence.Get(), fenceValueForSignal));
	}

	void Device_DX12::ExecuteComputeCommandBuffer(Ref<CommandBuffer> computeCommandBuffer)
	{
		DIRECTX12_ASSERT(computeCommandBuffer->CommandList->Close());
		ID3D12CommandList* const commandLists[]{ computeCommandBuffer->CommandList.Get() };
		m_ComputeCommandQueue->ExecuteCommandLists(_countof(commandLists), &commandLists[0]);

		u64 fenceValueForSignal = ++computeCommandBuffer->FenceValue;
		DIRECTX12_ASSERT(m_ComputeCommandQueue->Signal(computeCommandBuffer->Fence.Get(), fenceValueForSignal));
	}

	void Device_DX12::ExecuteCopyCommandBuffer(Ref<CommandBuffer> copyCommandBuffer)
	{
		DIRECTX12_ASSERT(copyCommandBuffer->CommandList->Close());
		ID3D12CommandList* const commandLists[]{ copyCommandBuffer->CommandList.Get() };
		m_CopyCommandQueue->ExecuteCommandLists(_countof(commandLists), &commandLists[0]);

		u64 fenceValueForSignal = ++copyCommandBuffer->FenceValue;
		DIRECTX12_ASSERT(m_CopyCommandQueue->Signal(copyCommandBuffer->Fence.Get(), fenceValueForSignal));
	}

	bool Device_DX12::Resize(const Event::EventData& data)
	{
		//Flush();
		u32 width = data.Data.u32[0];
		u32 height = data.Data.u32[1];
		LOG_INFO("Resizeing to: %u, %u", width, height);

		if (width == 0) width = 1;
		if (height == 0) height = 1;
		RecreateBackBuffers(width, height);
		return false;
	}

	void Device_DX12::FlagResrouceForDeletion(Ref<Resource>& resource)
	{
		if (!s_Device)
		{
			ASSERT(false, "Device not initialized!");
		}

		s_Device->m_ResourcesToBeDeleted.push_back(resource);
	}

	void Device_DX12::FlagPipelineStateForDeletion(ComPtr<ID3D12PipelineState>& pipelineState)
	{
		if (!s_Device)
		{
			ASSERT(false, "Device not initialized!");
		}

		s_Device->m_PipelineStatesToBeDeleted.push_back(pipelineState);
	}

	void Device_DX12::FlagRtvDescriptorHandleForDeletion(DescriptorHandle& handle)
	{
		if (!s_Device)
		{
			ASSERT(false, "Device not initialized!");
		}

		s_Device->m_RtvDescHeapHandlesToBeDeleted.push_back(handle);
	}
	void Device_DX12::FlagDsvDescriptorHandleForDeletion(DescriptorHandle& handle)
	{
		if (!s_Device)
		{
			ASSERT(false, "Device not initialized!");
		}

		s_Device->m_DsvDescHeapHandlesToBeDeleted.push_back(handle);
	}
	void Device_DX12::FlagSrvDescriptorHandleForDeletion(DescriptorHandle& handle)
	{
		if (!s_Device)
		{
			ASSERT(false, "Device not initialized!");
		}

		s_Device->m_SrvDescHeapHandlesToBeDeleted.push_back(handle);
	}

#pragma endregion

	// Private Methods

#pragma region Device_DX12 Private Methods

	void Device_DX12::CreateDevice()
	{
		if (m_Device)
		{
			ASSERT(false, "Device already exists! cannot create it again!");
		}

		u32 createFactoryFlags = 0;

#if defined(DX12_DEBUG_LAYER)
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
		ASSERT(maxFeatureLevel >= c_MinimumFeatureLevel, "Device does not support minimum feature level!");
		if (maxFeatureLevel < c_MinimumFeatureLevel) return;

		DIRECTX12_ASSERT(D3D12CreateDevice(adapter.Get(), maxFeatureLevel, IID_PPV_ARGS(&m_Device)));

		NAME_DIRECTX12_OBJECT(m_Device, "Main Device");

#if defined(DX12_DEBUG_LAYER)
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

		ASSERT(false, "Could not find a suiting adapter!");
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

	Texture Device_DX12::CreateTexture(const TextureDesc& desc, DXGI_FORMAT format,
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
			texture_DX12->ShaderResourceView = m_SrvDescHeap.Allocate();

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
				texture_DX12->ShaderResourceView.CPU
			);
		}

		// UAV
		if (textureResourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS && desc.Format != DataFormat::R8G8B8A8_SRGB)
		{
			texture_DX12->UnorderedAccessView = m_SrvDescHeap.Allocate();

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
				texture_DX12->UnorderedAccessView.CPU
			);
		}

		// Mips
		{
			texture_DX12->MipShaderResourceViews.resize(desc.NumMips);
			texture_DX12->MipUnorderedAccessViews.resize(desc.NumMips);
			texture_DX12->TextureResource->SubResourceStates.resize(desc.NumMips);
			for (u32 i = 0; i < desc.NumMips; i++)
			{
				texture_DX12->MipShaderResourceViews[i] = m_SrvDescHeap.Allocate();
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
					texture_DX12->MipShaderResourceViews[i].CPU
				);
				if (textureResourceDesc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)
				{
					texture_DX12->MipUnorderedAccessViews[i] = m_SrvDescHeap.Allocate();
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
						texture_DX12->MipUnorderedAccessViews[i].CPU
					);
				}

				texture_DX12->TextureResource->SubResourceStates[i] = texture_DX12->TextureResource->CurrentState;
			}

		}

		//NAME_DIRECTX12_OBJECT(texture_DX12->TextureResource->ResourceDX12, desc.Name);

		Texture texture(desc);
		texture.Data = texture_DX12;

		return texture;
	}

	void Device_DX12::CopyToResource(ComPtr<ID3D12Resource>& resource,
		D3D12_SUBRESOURCE_DATA& subResourceData, u32 subResource, u32 offset)
	{
		ComPtr<ID3D12Resource> intermediateResource;

		UINT64 requiredSize = GetRequiredIntermediateSize(resource.Get(), 0, 1);
		CD3DX12_HEAP_PROPERTIES heapProps(D3D12_HEAP_TYPE_UPLOAD);
		CD3DX12_RESOURCE_DESC buffer = CD3DX12_RESOURCE_DESC::Buffer(requiredSize);
		m_Device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &buffer,
			D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&intermediateResource));

		Ref<CommandBuffer> copyCommandList = GetFreeCopyCommandBuffer();

		UpdateSubresources(copyCommandList->CommandList.Get(), resource.Get(),
			intermediateResource.Get(), offset, subResource, 1, &subResourceData);

		ExecuteCopyCommandBuffer(copyCommandList);

		copyCommandList->Wait(m_FenceEvent);

	}

	void Device_DX12::RecreateBackBuffers(u32 width, u32 height)
	{
		ASSERT(width > 0, "Width must be bigger than 0!");
		ASSERT(height > 0, "Height must be bigger than 0!");
		m_BackBuffers.clear();
		ProcessDeferredReleases();

		DXGI_SWAP_CHAIN_DESC1 desc{};
		m_SwapChain->GetDesc1(&desc);
		m_SwapChain->ResizeBuffers(m_NumBackBuffers, width, height, desc.Format, desc.Flags);

		m_BackBuffers.resize(m_NumBackBuffers);

		for (u32 i = 0; i < m_NumBackBuffers; i++)
		{
			Ref<RenderTarget_DX12> renderTarget_DX12 = CreateRef<RenderTarget_DX12>();
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

			Texture texture(textureDesc);
			texture.Data = texture_DX12;

			renderTarget_DX12->RenderTargetViews[0] = GetRtvHeap().Allocate();

			D3D12_RENDER_TARGET_VIEW_DESC renderTargetViewDesc{};
			renderTargetViewDesc.Format = format;
			renderTargetViewDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			m_Device->CreateRenderTargetView(texture_DX12->TextureResource->ResourceDX12.Get(), &renderTargetViewDesc, renderTarget_DX12->RenderTargetViews[0].CPU);
			NAME_DIRECTX12_OBJECT_INDEXED(texture_DX12->TextureResource->ResourceDX12, i, "BackBuffer");

			renderTarget_DX12->Rect.left = 0;
			renderTarget_DX12->Rect.top = 0;
			renderTarget_DX12->Rect.right = static_cast<i32>(width);
			renderTarget_DX12->Rect.bottom = static_cast<i32>(height);

			renderTarget_DX12->ViewPort.TopLeftX = 0.f;
			renderTarget_DX12->ViewPort.TopLeftY = 0.f;
			renderTarget_DX12->ViewPort.Width = static_cast<f32>(width);
			renderTarget_DX12->ViewPort.Height = static_cast<f32>(height);
			renderTarget_DX12->ViewPort.MinDepth = 0.f;
			renderTarget_DX12->ViewPort.MaxDepth = 1.f;

			m_BackBuffers[i] = RenderTarget(renderTargetDesc);
			m_BackBuffers[i].Data = renderTarget_DX12;
			m_BackBuffers[i].RenderTextures[0] = texture;
		}

		m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();
	}

	void __declspec(noinline) Device_DX12::ProcessDeferredReleases()
	{
		std::lock_guard<std::mutex> lock(m_DeferredReleasesMutex);

		for (DescriptorHandle& handle : m_RtvDescHeapHandlesToBeDeleted)
		{
			m_RtvDescHeap.Free(handle);
		}
		m_RtvDescHeapHandlesToBeDeleted.clear();
		for (DescriptorHandle& handle : m_DsvDescHeapHandlesToBeDeleted)
		{
			m_DsvDescHeap.Free(handle);

		}
		m_DsvDescHeapHandlesToBeDeleted.clear();
		for (DescriptorHandle& handle : m_SrvDescHeapHandlesToBeDeleted)
		{
			m_SrvDescHeap.Free(handle);
		}
		m_SrvDescHeapHandlesToBeDeleted.clear();


		m_RtvDescHeap.ProcessDeferredFree();
		m_DsvDescHeap.ProcessDeferredFree();
		m_SrvDescHeap.ProcessDeferredFree();

		if (m_ResourcesToBeDeleted.size() > 0 || m_PipelineStatesToBeDeleted.size() > 0)
		{
			Flush();
			m_ResourcesToBeDeleted.clear();
			m_PipelineStatesToBeDeleted.clear();
		}
	}

	Ref<Buffer_DX12> Device_DX12::CreateBuffer(const BufferDesc& desc)
	{
		// Create the buffer
		{ // Validity check
			std::string failureReason{};
			ASSERT(desc.IsValid(&failureReason), failureReason.c_str());
		}
		Ref<Buffer_DX12> buffer_DX12 = CreateRef<Buffer_DX12>();

		if (desc.Type == BufferType::Constant)
		{
			buffer_DX12->AllocatedMemorySize = (desc.Size + 255) & ~255;
		}
		else
		{
			buffer_DX12->AllocatedMemorySize = desc.NumElements * desc.Stride;
		}

		// Creating the resource
		D3D12_HEAP_TYPE heapType = desc.MemoryType == MemoryType::Dedicated ? D3D12_HEAP_TYPE_DEFAULT : D3D12_HEAP_TYPE_UPLOAD;
		CD3DX12_HEAP_PROPERTIES heapProps(heapType);
		D3D12_RESOURCE_FLAGS resourceFlags = D3D12_RESOURCE_FLAG_NONE;
		if (desc.CanReadWrite)
		{
			resourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		}
		CD3DX12_RESOURCE_DESC bufferDescDX12 = CD3DX12_RESOURCE_DESC::Buffer(buffer_DX12->AllocatedMemorySize, resourceFlags);
		buffer_DX12->BufferResource = CreateRef<Resource>();
		buffer_DX12->BufferResource->CurrentState = desc.MemoryType == MemoryType::Dedicated ? D3D12_RESOURCE_STATE_COMMON : D3D12_RESOURCE_STATE_GENERIC_READ;
		m_Device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufferDescDX12, buffer_DX12->BufferResource->CurrentState,
			nullptr, IID_PPV_ARGS(&buffer_DX12->BufferResource->ResourceDX12));

		if (desc.Type == BufferType::Constant)
		{
			buffer_DX12->ConstantBufferView = m_SrvDescHeap.Allocate();

			D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
			cbvDesc.BufferLocation = buffer_DX12->BufferResource->ResourceDX12->GetGPUVirtualAddress();
			cbvDesc.SizeInBytes = static_cast<u32>(buffer_DX12->AllocatedMemorySize);
			m_Device->CreateConstantBufferView(&cbvDesc, buffer_DX12->ConstantBufferView.CPU);
		}
		else
		{
			buffer_DX12->ShaderResourceView = m_SrvDescHeap.Allocate();
			D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
			srvDesc.Format = DXGI_FORMAT_UNKNOWN;
			srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
			srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			srvDesc.Buffer.FirstElement = 0;
			srvDesc.Buffer.NumElements = desc.NumElements;
			srvDesc.Buffer.StructureByteStride = desc.Stride;
			srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
			m_Device->CreateShaderResourceView(buffer_DX12->BufferResource->ResourceDX12.Get(),
				&srvDesc, buffer_DX12->ShaderResourceView.CPU);

			if (desc.CanReadWrite)
			{
				buffer_DX12->ReadWriteBufferView = m_SrvDescHeap.Allocate();

				D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
				uavDesc.Format = DXGI_FORMAT_UNKNOWN;
				uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
				uavDesc.Buffer.CounterOffsetInBytes = 0;
				uavDesc.Buffer.FirstElement = 0;
				uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
				uavDesc.Buffer.NumElements = desc.NumElements;
				uavDesc.Buffer.StructureByteStride = desc.Stride;

				m_Device->CreateUnorderedAccessView(buffer_DX12->BufferResource->ResourceDX12.Get(),
					nullptr, &uavDesc, buffer_DX12->ReadWriteBufferView.CPU);
			}
		}

		return buffer_DX12;
	}

#pragma endregion

}

#endif // WIN32

