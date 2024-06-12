#ifdef WIN32

#include "Device_DX12.h"
#include "Platform/Platform.h"
#include "CommandList_DX12.h"

#include "Objects_DX12.h"

#include <dxgi1_6.h>
#include "d3dx12.h"
#include <d3dcompiler.h>
#include <backends/imgui_impl_win32.h>
#include <backends/imgui_impl_dx12.h>

#define SizeOfInUint32(obj) ((sizeof(obj) - 1) / sizeof(UINT32) + 1)

const wchar_t* c_hitGroupName = L"MyHitGroup";
const wchar_t* c_raygenShaderName = L"MyRaygenShader";
const wchar_t* c_closestHitShaderName = L"MyClosestHitShader";
const wchar_t* c_missShaderName = L"MyMissShader";

namespace Gecko { namespace DX12
{
	void OnResize(u32 width, u32 height, void* Device)
	{
		Device_DX12* device = reinterpret_cast<Device_DX12*>(Device);
		device->Resize(width, height);
	}

	template<typename T>
	void CreateCommandBuffers(ComPtr<ID3D12Device8> device, ComPtr<ID3D12CommandQueue>& commandQueue, T& commandBuffers, D3D12_COMMAND_LIST_TYPE type);

	Device_DX12::Device_DX12()
	{
		CreateDevice();

		const Platform::AppInfo& info = Platform::GetAppInfo();
		HWND window = reinterpret_cast<HWND>(Platform::GetWindowData());
		
		m_NumBackBuffers = info.NumBackBuffers;

		// Create the command queue, allocator and list for the Graphics
		CreateCommandBuffers(m_Device, m_GraphicsCommandQueue, m_GraphicsCommandBuffers, D3D12_COMMAND_LIST_TYPE_DIRECT);
		CreateCommandBuffers(m_Device, m_ComputeCommandQueue, m_ComputeCommandBuffers, D3D12_COMMAND_LIST_TYPE_COMPUTE);
		CreateCommandBuffers(m_Device, m_CopyCommandQueue, m_CopyCommandBuffers, D3D12_COMMAND_LIST_TYPE_COPY);

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
		DIRECTX12_ASSERT(m_Factory->CreateSwapChainForHwnd(m_GraphicsCommandQueue.Get(), window, &swapchainDesc, nullptr, nullptr, &swapChain));
		DIRECTX12_ASSERT(m_Factory->MakeWindowAssociation(window, DXGI_MWA_NO_ALT_ENTER));

		DIRECTX12_ASSERT(swapChain->QueryInterface(IID_PPV_ARGS(&m_SwapChain)));

		m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();
		
		m_BackBuffers.resize(m_NumBackBuffers);
		// Createing the render targets
		for (u32 i = 0; i < m_NumBackBuffers; i++)
		{

			Ref<RenderTarget_DX12> data = CreateRef<RenderTarget_DX12>();
			data->RenderTargetResources[0] = CreateRef<Resource>();

			DIRECTX12_ASSERT(m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&data->RenderTargetResources[0]->Resource)));

			RenderTargetDesc renderTargetDesc;
			renderTargetDesc.RenderTargetFormats[0] = m_BackBufferFormat;
			renderTargetDesc.NumRenderTargets = 1;
			renderTargetDesc.DepthStencilFormat = Format::R32_FLOAT;
			renderTargetDesc.Width = info.Width;
			renderTargetDesc.Height = info.Height;
			renderTargetDesc.RenderTargetClearValues[0].Values[0] = 0.5f;
			renderTargetDesc.RenderTargetClearValues[0].Values[1] = 0.0f;
			renderTargetDesc.RenderTargetClearValues[0].Values[2] = 0.5f;
			renderTargetDesc.RenderTargetClearValues[0].Values[3] = 1.0f;
			renderTargetDesc.DepthTargetClearValue.Depth = 1.0f;
			
			m_BackBuffers[i] = CreateRenderTarget(renderTargetDesc, data);

		}

		Gecko::Platform::AddResizeEvent(&OnResize, this);

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
		ImGui::GetStyle().Alpha = .0f;

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
		imGuiHandle = m_SrvDescHeap.Allocate();
		ImGui_ImplDX12_Init(
			GetDevice().Get(), 
			info.NumBackBuffers,
			FormatToD3D12Format(m_BackBufferFormat),
			m_SrvDescHeap.Heap(),
			imGuiHandle.CPU,
			imGuiHandle.GPU
		);

		ImGui_ImplDX12_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
	};
	
	Device_DX12::~Device_DX12()
	{
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
		DIRECTX12_ASSERT(m_SwapChain->Present(1, 0));
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

	Ref<RenderTarget> Device_DX12::GetCurrentBackBuffer()
	{
		return m_BackBuffers[m_CurrentBackBufferIndex];
	}

	Ref<RenderTarget> Device_DX12::CreateRenderTarget(const RenderTargetDesc& desc)
	{
		Ref<RenderTarget_DX12> renderTargetDX12 = CreateRef<RenderTarget_DX12>();

		return CreateRenderTarget(desc, renderTargetDX12);
	}

	Ref<RenderTarget> Device_DX12::CreateRenderTarget(const RenderTargetDesc& desc, Ref<RenderTarget_DX12>& renderTargetDX12)
	{

		DescriptorHandle renderTargetSrvs[8];
		DescriptorHandle depthStencilSrv;

		
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

			D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
				format,
				(u64)desc.Width,
				(u32)desc.Height,
				1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
			);
			CD3DX12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

			if(renderTargetDX12->RenderTargetResources[i] == nullptr)
			{ 
				renderTargetDX12->RenderTargetResources[i] = CreateRef<Resource>();

				m_Device->CreateCommittedResource(
					&heapProperties,
					D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES,
					&resourceDesc,
					D3D12_RESOURCE_STATE_COMMON,
					&clearValue,
					IID_PPV_ARGS(&renderTargetDX12->RenderTargetResources[i]->Resource)
				);
				NAME_DIRECTX12_OBJECT(renderTargetDX12->RenderTargetResources[i]->Resource, "RenderTarget");
			}

			renderTargetDX12->RenderTargetResources[i]->CurrentState = D3D12_RESOURCE_STATE_COMMON;

			renderTargetDX12->rtvs[i] = GetRtvHeap().Allocate();

			D3D12_RENDER_TARGET_VIEW_DESC renderTargetDesc{};
			renderTargetDesc.Format = format;
			renderTargetDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
			m_Device->CreateRenderTargetView(renderTargetDX12->RenderTargetResources[i]->Resource.Get(), &renderTargetDesc, renderTargetDX12->rtvs[i].CPU);

			if (desc.AllowRenderTargetTexture)
			{
				D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
				srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				srvDesc.Format = format;
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				srvDesc.Texture2D.MipLevels = 1;

				renderTargetDX12->renderTargetSrvs[i] = m_SrvDescHeap.Allocate();
				m_Device->CreateShaderResourceView(
					renderTargetDX12->RenderTargetResources[i]->Resource.Get(),
					&srvDesc,
					renderTargetDX12->renderTargetSrvs[i].CPU
				);

				D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
				uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
				uavDesc.Format = format;
				uavDesc.Texture2D.MipSlice = 0;
				uavDesc.Texture2D.PlaneSlice = 0;
				uavDesc.Texture2DArray.FirstArraySlice = 0;
				uavDesc.Texture2DArray.ArraySize = 1;
				
				renderTargetDX12->renderTargetUavs[i] = m_SrvDescHeap.Allocate();
				m_Device->CreateUnorderedAccessView(
					renderTargetDX12->RenderTargetResources[i]->Resource.Get(),
					nullptr,
					&uavDesc,
					renderTargetDX12->renderTargetUavs[i].CPU
				);
			}
		}

		if(desc.DepthStencilFormat != Format::None)
		{
			DXGI_FORMAT format = DXGI_FORMAT_D32_FLOAT;

			D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
				format,
				(u64)desc.Width,
				(u32)desc.Height,
				1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL
			);


			CD3DX12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
			
			D3D12_CLEAR_VALUE clearValue;
			clearValue.Format = DXGI_FORMAT_D32_FLOAT;
			clearValue.DepthStencil.Depth = desc.DepthTargetClearValue.Depth;
			clearValue.DepthStencil.Stencil = 0;

			renderTargetDX12->DepthBufferResource = CreateRef<Resource>();

			renderTargetDX12->DepthBufferResource->CurrentState = D3D12_RESOURCE_STATE_COMMON;

			m_Device->CreateCommittedResource(
				&heapProperties,
				D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES,
				&resourceDesc,
				renderTargetDX12->DepthBufferResource->CurrentState,
				&clearValue,
				IID_PPV_ARGS(&renderTargetDX12->DepthBufferResource->Resource)
			);
			NAME_DIRECTX12_OBJECT(renderTargetDX12->DepthBufferResource->Resource, "DepthBuffer");

			renderTargetDX12->dsv = GetDsvHeap().Allocate();
			
			D3D12_DEPTH_STENCIL_VIEW_DESC depthStencilDesc;
			depthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT;
			depthStencilDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
			depthStencilDesc.Texture2D.MipSlice = 0;
			depthStencilDesc.Flags = D3D12_DSV_FLAG_NONE;
			m_Device->CreateDepthStencilView(renderTargetDX12->DepthBufferResource->Resource.Get(), &depthStencilDesc, renderTargetDX12->dsv.CPU);


			if (desc.AllowDepthStencilTexture)
			{
				D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
				srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
				srvDesc.Format = FormatToD3D12Format(desc.DepthStencilFormat);
				srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
				srvDesc.Texture2D.MipLevels = 1;

				renderTargetDX12->depthStencilSrv = m_SrvDescHeap.Allocate();
				m_Device->CreateShaderResourceView(
					renderTargetDX12->DepthBufferResource->Resource.Get(),
					&srvDesc,
					renderTargetDX12->depthStencilSrv.CPU
				);
			}
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

		Ref<RenderTarget> renderTarget = CreateRef<RenderTarget>();
		renderTarget->Desc = desc;
		renderTarget->Data = renderTargetDX12;

		return renderTarget;
	}

	Ref<VertexBuffer> Device_DX12::CreateVertexBuffer(const VertexBufferDesc& desc)
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
			IID_PPV_ARGS(&vertexBuffer_DX12->VertexBufferResource->Resource)
		);

		D3D12_SUBRESOURCE_DATA subresourceData = {};
		subresourceData.pData = desc.VertexData;
		subresourceData.RowPitch = bufferSize;
		subresourceData.SlicePitch = subresourceData.RowPitch;

		CopyToResource(vertexBuffer_DX12->VertexBufferResource->Resource, subresourceData);

		// Create the vertex buffer view.
		vertexBuffer_DX12->VertexBufferView.BufferLocation = vertexBuffer_DX12->VertexBufferResource->Resource->GetGPUVirtualAddress();
		vertexBuffer_DX12->VertexBufferView.SizeInBytes = static_cast<u32>(bufferSize);
		vertexBuffer_DX12->VertexBufferView.StrideInBytes = desc.Layout.Stride;

		Ref<VertexBuffer> vertexBuffer = CreateRef<VertexBuffer>();
		vertexBuffer->Desc = desc;
		vertexBuffer->Data = vertexBuffer_DX12;
		

		return vertexBuffer;
	}

	Ref<IndexBuffer> Device_DX12::CreateIndexBuffer(const IndexBufferDesc& desc)
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
			IID_PPV_ARGS(&indexBuffer_DX12->IndexBufferResource->Resource)
		);

		NAME_DIRECTX12_OBJECT(indexBuffer_DX12->IndexBufferResource->Resource, "IndexBuffer");

		D3D12_SUBRESOURCE_DATA subresourceData = {};
		subresourceData.pData = desc.IndexData;
		subresourceData.RowPitch = bufferSize;
		subresourceData.SlicePitch = subresourceData.RowPitch;

		CopyToResource(indexBuffer_DX12->IndexBufferResource->Resource, subresourceData);

		indexBuffer_DX12->IndexBufferView.BufferLocation = indexBuffer_DX12->IndexBufferResource->Resource->GetGPUVirtualAddress();
		indexBuffer_DX12->IndexBufferView.SizeInBytes = static_cast<u32>(bufferSize);
		indexBuffer_DX12->IndexBufferView.Format = FormatToD3D12Format(desc.IndexFormat);

		Ref<IndexBuffer> indexBuffer = CreateRef<IndexBuffer>();
		indexBuffer->Desc = desc;
		indexBuffer->Data = indexBuffer_DX12;

		return indexBuffer;
	}

	Ref<Texture> Device_DX12::CreateTexture(const TextureDesc& desc)
	{

		Ref<Texture_DX12> texture_DX12 = CreateRef<Texture_DX12>();

		CD3DX12_RESOURCE_DESC textureResourceDesc;

		switch (desc.Type)
		{
		case TextureType::Tex1D:
			textureResourceDesc = CD3DX12_RESOURCE_DESC::Tex1D(
				FormatToD3D12Format(desc.Format),
				desc.Width,
				1,
				static_cast<u16>(desc.NumMips)
			);
			break;
		case TextureType::Tex2D:
			textureResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
				FormatToD3D12Format(desc.Format),
				desc.Width,
				desc.Height,
				1,
				static_cast<u16>(desc.NumMips)
			);
			break;
		case TextureType::Tex3D:
			textureResourceDesc = CD3DX12_RESOURCE_DESC::Tex3D(
				FormatToD3D12Format(desc.Format),
				desc.Width,
				desc.Height,
				static_cast<u16>(desc.Depth),
				static_cast<u16>(desc.NumMips)
			);
			break;
		case TextureType::TexCube:
			textureResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
				FormatToD3D12Format(desc.Format),
				desc.Width,
				desc.Height,
				6,
				static_cast<u16>(desc.NumMips)
			);
			break;
		case TextureType::Tex1DArray:
			textureResourceDesc = CD3DX12_RESOURCE_DESC::Tex1D(
				FormatToD3D12Format(desc.Format),
				desc.Width,
				static_cast<u16>(desc.NumArraySlices),
				static_cast<u16>(desc.NumMips)
			);
			break;
		case TextureType::Tex2DArray:
			textureResourceDesc = CD3DX12_RESOURCE_DESC::Tex2D(
				FormatToD3D12Format(desc.Format),
				desc.Width,
				desc.Height,
				static_cast<u16>(desc.NumArraySlices),
				static_cast<u16>(desc.NumMips)
			);
			break;
		}

		CD3DX12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);

		textureResourceDesc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		texture_DX12->TextureResource = CreateRef<Resource>();

		texture_DX12->TextureResource->CurrentState = D3D12_RESOURCE_STATE_COMMON;

		DIRECTX12_ASSERT(m_Device->CreateCommittedResource(
			&heapProps,
			D3D12_HEAP_FLAG_NONE,
			&textureResourceDesc,
			texture_DX12->TextureResource->CurrentState,
			nullptr,
			IID_PPV_ARGS(&texture_DX12->TextureResource->Resource)
		));

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
				texture_DX12->TextureResource->Resource.Get(),
				&srvDesc,
				texture_DX12->srv.CPU
			);
		}

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

			if (desc.Format != Format::R8G8B8A8_SRGB)
			{

				m_Device->CreateUnorderedAccessView(
					texture_DX12->TextureResource->Resource.Get(),
					nullptr,
					&uavDesc,
					texture_DX12->uav.CPU
				);
			}
		}


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
					texture_DX12->TextureResource->Resource.Get(), 
					&srvDesc, 
					texture_DX12->mipSrvs[i].CPU
				);

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
					texture_DX12->TextureResource->Resource.Get(),
					nullptr, 
					&uavDesc, 
					texture_DX12->mipUavs[i].CPU
				);
				texture_DX12->TextureResource->subResourceStates[i] = texture_DX12->TextureResource->CurrentState;
			}

		}

		NAME_DIRECTX12_OBJECT(texture_DX12->TextureResource->Resource, "texture");

		texture_DX12->device = this;

		Ref<Texture> texture = CreateRef<Texture>();
		texture->Desc = desc;
		texture->Data = texture_DX12;

		return texture;
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
					desc.DynamicCallData.Size/4,
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

			InputLayout = { inputLayoutElements.data(), static_cast<u32>(inputLayoutElements.size())};
			
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
			std::string vertexPath = desc.VertexShaderPath;
			vertexPath += ".cso";
			{
				std::wstring vertexSource;
				vertexSource.assign(vertexPath.begin(), vertexPath.end());

				D3DReadFileToBlob(vertexSource.c_str(), &vertexShaderBlob);

				VS = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
			}
			piplineStateStream2.VS = VS;
		}

		CD3DX12_PIPELINE_STATE_STREAM_PS PS;
		ComPtr<ID3DBlob> pixelShaderBlob;
		if (desc.PixelShaderPath != nullptr)
		{
			std::string pixelPath = desc.PixelShaderPath;
			pixelPath += ".cso";
			{
				std::wstring pixelSource;
				pixelSource.assign(pixelPath.begin(), pixelPath.end());

				D3DReadFileToBlob(pixelSource.c_str(), &pixelShaderBlob);
				PS = CD3DX12_SHADER_BYTECODE(pixelShaderBlob.Get());
			}
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
		if(desc.DepthStencilFormat != Format::None)
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
				CullModeToDirectX12CullMode(desc.CullMode) ,
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

		m_Device->CreateGraphicsPipelineState(&graphicsDesc, IID_PPV_ARGS(&graphicsPipeline_DX12->PipelineState));

		GraphicsPipeline graphicsPipeline;
		graphicsPipeline.Desc = desc;
		graphicsPipeline.Data = graphicsPipeline_DX12;

		return graphicsPipeline;
	}

	ComputePipeline Device_DX12::CreateComputePipeline(const ComputePipelineDesc& desc)
	{

		Ref<ComputePipeline_DX12> computePipeline_DX12 = CreateRef<ComputePipeline_DX12>();

		computePipeline_DX12->device = this;
		std::string computePath = desc.ComputeShaderPath;
		computePath += ".cso";

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
		ComPtr<ID3DBlob> vertexShaderBlob;
		{
			std::wstring vertexSource;
			vertexSource.assign(computePath.begin(), computePath.end());

			D3DReadFileToBlob(vertexSource.c_str(), &vertexShaderBlob);

			CS = CD3DX12_SHADER_BYTECODE(vertexShaderBlob.Get());
		}
		computeStateDesc.CS = CS;
		
		computeStateDesc.CachedPSO.pCachedBlob = nullptr;
		computeStateDesc.CachedPSO.CachedBlobSizeInBytes = 0;
		computeStateDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
		computeStateDesc.NodeMask = 0;

		m_Device->CreateComputePipelineState(&computeStateDesc, IID_PPV_ARGS(&computePipeline_DX12->PipelineState));

		ComputePipeline computePipeline;
		computePipeline.Desc = desc;
		computePipeline.Data = computePipeline_DX12;

		return computePipeline;
	}

	Ref<ConstantBuffer> Device_DX12::CreateConstantBuffer(const ConstantBufferDesc& desc)
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
			IID_PPV_ARGS(&constantBuffer_DX12->ConstantBufferResource->Resource)
		);

		constantBuffer_DX12->cbv = m_SrvDescHeap.Allocate();

		D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc;
		cbvDesc.BufferLocation = constantBuffer_DX12->ConstantBufferResource->Resource->GetGPUVirtualAddress();
		cbvDesc.SizeInBytes = static_cast<u32>(constantBuffer_DX12->MemorySize);
		m_Device->CreateConstantBufferView(&cbvDesc, constantBuffer_DX12->cbv.CPU);

		CD3DX12_RANGE readRange(0, 0);
		constantBuffer_DX12->ConstantBufferResource->Resource->Map(0, &readRange, &constantBuffer_DX12->GPUAddress);

		constantBuffer_DX12->device = this;

		Ref<ConstantBuffer> constantBuffer = CreateRef<ConstantBuffer>();
		constantBuffer->Desc = desc;
		constantBuffer->Data = constantBuffer_DX12;
		constantBuffer->Buffer = constantBuffer_DX12->GPUAddress;

		return constantBuffer;
	}

	RaytracingPipeline Device_DX12::CreateRaytracingPipeline(const RaytracingPipelineDesc& desc)
	{
		Ref<RaytracingPipeline_DX12> raytracingPipeline_DX12 = CreateRef<RaytracingPipeline_DX12>();
		// Create the Raytracing root signatures
		{
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

				u32 numRootParams = desc.NumConstantBuffers + desc.NumTextures + desc.NumUAVs + 1;
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
					raytracingPipeline_DX12->ConstantBufferIndices.push_back(descriptorIndex);
				}

				for (u32 i = 1; i < desc.NumTextures+1; i++)
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

					raytracingPipeline_DX12->TextureIndices.push_back(descriptorIndex);
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
					raytracingPipeline_DX12->UAVIndices.push_back(descriptorIndex);
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
					raytracingPipeline_DX12->ConstantBufferIndices.push_back(descriptorIndex);
				}

				// Initialize the TLAS as a srv at t0
				raytracingPipeline_DX12->TLASSlot = numRootParams - 1;
				rootParameters[raytracingPipeline_DX12->TLASSlot].InitAsShaderResourceView(0);

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
					IID_PPV_ARGS(&raytracingPipeline_DX12->RootSignature)
				));

				pRootSignature = raytracingPipeline_DX12->RootSignature.Get();
			}
		}

		// Create pipeline for raytracing
		{
			CD3DX12_STATE_OBJECT_DESC raytracingPipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

			// Load the library shader
			CD3DX12_DXIL_LIBRARY_SUBOBJECT* lib = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();

			std::wstring libSource = L"Shaders/Raytracing.cso";

			ComPtr<ID3DBlob> libShaderBlob;
			D3DReadFileToBlob(libSource.c_str(), &libShaderBlob);

			D3D12_SHADER_BYTECODE libdxil = CD3DX12_SHADER_BYTECODE(libShaderBlob.Get());
			lib->SetDXILLibrary(&libdxil);
			// Set the different raygen/hit/miss shaders
			{
				lib->DefineExport(c_raygenShaderName);
				lib->DefineExport(c_closestHitShaderName);
				lib->DefineExport(c_missShaderName);
			}

			// TriangleHitGroup
			CD3DX12_HIT_GROUP_SUBOBJECT* hitGroup = 
				raytracingPipeline.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
			hitGroup->SetClosestHitShaderImport(c_closestHitShaderName);
			hitGroup->SetHitGroupExport(c_hitGroupName);
			hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

			// config
			CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT* shaderConfig = 
				raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
			UINT payloadSize = 4 * sizeof(float);   // float4 color
			UINT attributeSize = 2 * sizeof(float); // float2 barycentrics
			shaderConfig->Config(payloadSize, attributeSize);

			//// Create local root signature
			//{
			//	auto localRootSignature = raytracingPipeline.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
			//	localRootSignature->SetRootSignature(m_raytracingLocalRootSignature.Get());
			//	// Shader association
			//	auto rootSignatureAssociation = raytracingPipeline.CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
			//	rootSignatureAssociation->SetSubobjectToAssociate(*localRootSignature);
			//	rootSignatureAssociation->AddExport(c_raygenShaderName);
			//}

			// Create global root signature
			auto globalRootSignature = raytracingPipeline.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
			globalRootSignature->SetRootSignature(raytracingPipeline_DX12->RootSignature.Get());

			// Set pipeline config for max recursion depth
			auto pipelineConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
			UINT maxRecursionDepth = 1; // ~ primary rays only. 
			pipelineConfig->Config(maxRecursionDepth);

			
			DIRECTX12_ASSERT_MSG(m_Device->CreateStateObject(
				raytracingPipeline, 
				IID_PPV_ARGS(&raytracingPipeline_DX12->StateObject)),
				"Couldn't create DirectX Raytracing state object.\n"
			);
		}

		// Build shader tables
		{
			void* rayGenShaderIdentifier;
			void* missShaderIdentifier;
			void* hitGroupShaderIdentifier;

			auto GetShaderIdentifiers = [&](auto* stateObjectProperties)
			{
				rayGenShaderIdentifier = stateObjectProperties->GetShaderIdentifier(c_raygenShaderName);
				missShaderIdentifier = stateObjectProperties->GetShaderIdentifier(c_missShaderName);
				hitGroupShaderIdentifier = stateObjectProperties->GetShaderIdentifier(c_hitGroupName);
			};

			// Get shader identifiers.
			UINT shaderIdentifierSize;
			{
				ComPtr<ID3D12StateObjectProperties> stateObjectProperties;
				DIRECTX12_ASSERT(raytracingPipeline_DX12->StateObject.As(&stateObjectProperties));
				GetShaderIdentifiers(stateObjectProperties.Get());
				shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
			}
			
			// Ray gen shader table
			{
				
				UINT numShaderRecords = 1;
				UINT shaderRecordSize = shaderIdentifierSize;
				ShaderTable rayGenShaderTable(m_Device.Get(), numShaderRecords, shaderRecordSize, L"RayGenShaderTable");
				rayGenShaderTable.push_back(ShaderRecord(rayGenShaderIdentifier, shaderIdentifierSize));
				raytracingPipeline_DX12->RayGenShaderTable = rayGenShaderTable.GetResource();
			}

			// Miss shader table
			{
				UINT numShaderRecords = 1;
				UINT shaderRecordSize = shaderIdentifierSize;
				ShaderTable missShaderTable(m_Device.Get(), numShaderRecords, shaderRecordSize, L"MissShaderTable");
				missShaderTable.push_back(ShaderRecord(missShaderIdentifier, shaderIdentifierSize));
				raytracingPipeline_DX12->MissShaderTable = missShaderTable.GetResource();
			}

			// Hit group shader table
			{
				UINT numShaderRecords = 1;
				UINT shaderRecordSize = shaderIdentifierSize;
				ShaderTable hitGroupShaderTable(m_Device.Get(), numShaderRecords, shaderRecordSize, L"HitGroupShaderTable");
				hitGroupShaderTable.push_back(ShaderRecord(hitGroupShaderIdentifier, shaderIdentifierSize));
				raytracingPipeline_DX12->HitGroupShaderTable = hitGroupShaderTable.GetResource();
			}
		}

		RaytracingPipeline raytracingPipeline;

		raytracingPipeline.Desc = desc;
		raytracingPipeline.Data = raytracingPipeline_DX12;

		return raytracingPipeline;
	}

	BLAS Device_DX12::CreateBLAS(const BLASDesc& desc)
	{
		Ref<BLAS_DX12> blas_DX12 = CreateRef<BLAS_DX12>();

		VertexBuffer_DX12* vertexBuffer_DX12 = (VertexBuffer_DX12*)desc.VertexBuffer->Data.get();
		IndexBuffer_DX12* indexBuffer_DX12 = (IndexBuffer_DX12*)desc.IndexBuffer->Data.get();

		Ref<CommandBuffer> commandBuffer = GetFreeGraphicsCommandBuffer();
				
		// Reset the command list for the acceleration structure construction.
				
		D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
		geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
		geometryDesc.Triangles.IndexBuffer = indexBuffer_DX12->IndexBufferResource->Resource->GetGPUVirtualAddress();
		geometryDesc.Triangles.IndexCount = desc.IndexBuffer->Desc.NumIndices;
		geometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
		
		geometryDesc.Triangles.Transform3x4 = 0;
		
		geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
		geometryDesc.Triangles.VertexCount = desc.VertexBuffer->Desc.NumVertices;
		geometryDesc.Triangles.VertexBuffer.StartAddress = vertexBuffer_DX12->VertexBufferResource->Resource->GetGPUVirtualAddress();
		geometryDesc.Triangles.VertexBuffer.StrideInBytes = desc.VertexBuffer->Desc.Layout.Stride;
				
		// Mark the geometry as opaque. 
		// PERFORMANCE TIP: mark geometry as opaque whenever applicable as it can enable important ray processing optimizations.
		// Note: When rays encounter opaque geometry an any hit shader will not be executed whether it is present or not.
		geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
				
		// Get required sizes for an acceleration structure.
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelPrebuildInfo = {};
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS bottomLevelInputs = {};
		bottomLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		bottomLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
		bottomLevelInputs.Flags = buildFlags;
		bottomLevelInputs.NumDescs = 1;
		bottomLevelInputs.pGeometryDescs = &geometryDesc;
		m_Device->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &bottomLevelPrebuildInfo);
		ASSERT(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);
				
		ComPtr<ID3D12Resource> scratchResource;
		{
			CD3DX12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
			CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bottomLevelPrebuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
			DIRECTX12_ASSERT(m_Device.Get()->CreateCommittedResource(
				&uploadHeapProperties,
				D3D12_HEAP_FLAG_NONE,
				&bufferDesc,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				nullptr,
				IID_PPV_ARGS(&scratchResource)));

			scratchResource->SetName(L"ScratchResource");
		}

		// Allocate resources for acceleration structures.
		// Acceleration structures can only be placed in resources that are created in the default heap (or custom heap equivalent). 
		// Default heap is OK since the application doesn’t need CPU read/write access to them. 
		// The resources that will contain acceleration structures must be created in the state D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, 
		// and must have resource flag D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS. The ALLOW_UNORDERED_ACCESS requirement simply acknowledges both: 
		//  - the system will be doing this type of access in its implementation of acceleration structure builds behind the scenes.
		//  - from the app point of view, synchronization of writes/reads to acceleration structures is accomplished using UAV barriers.
		{
			D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
			
			{
				CD3DX12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
				CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
				DIRECTX12_ASSERT(m_Device.Get()->CreateCommittedResource(
					&uploadHeapProperties,
					D3D12_HEAP_FLAG_NONE,
					&bufferDesc,
					initialResourceState,
					nullptr,
					IID_PPV_ARGS(&blas_DX12->Resource)));

				blas_DX12->Resource->SetName(L"BottomLevelAccelerationStructure");
			}
		}
				
		// Create an instance desc for the bottom-level acceleration structure.
				
		// Bottom Level Acceleration Structure desc
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
		{
			bottomLevelBuildDesc.Inputs = bottomLevelInputs;
			bottomLevelBuildDesc.ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress();
			bottomLevelBuildDesc.DestAccelerationStructureData = blas_DX12->Resource->GetGPUVirtualAddress();
		}
		
		// Build the acceleration structure
		commandBuffer->CommandList.Get()->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);
		CD3DX12_RESOURCE_BARRIER uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(blas_DX12->Resource.Get());
		commandBuffer->CommandList->ResourceBarrier(
			1,
			&uavBarrier
		);

		// Kick off acceleration structure construction.
		ExecuteGraphicsCommandBuffer(commandBuffer);
		Flush();

		desc;
		BLAS blas;
		blas.Desc = desc;
		blas.Data = blas_DX12;

		return blas;
	}

	TLAS Device_DX12::CreateTLAS(const TLASRefitDesc& desc)
	{
		
		Ref<TLAS_DX12> tlas_DX12 = CreateRef<TLAS_DX12>();

		Ref<CommandBuffer> commandBuffer = GetFreeGraphicsCommandBuffer();

		// Get required sizes for an acceleration structure.
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS topLevelInputs = {};
		topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
		topLevelInputs.Flags = buildFlags;
		topLevelInputs.NumDescs = static_cast<u32>(desc.BLASInstances.size());
		topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo = {};
		m_Device->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &topLevelPrebuildInfo);
		ASSERT(topLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

		ComPtr<ID3D12Resource> scratchResource;
		{
			CD3DX12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
			CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(topLevelPrebuildInfo.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
			DIRECTX12_ASSERT(m_Device.Get()->CreateCommittedResource(
				&uploadHeapProperties,
				D3D12_HEAP_FLAG_NONE,
				&bufferDesc,
				D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
				nullptr,
				IID_PPV_ARGS(&scratchResource)));
			scratchResource->SetName(L"ScratchResource");
		}

		// Allocate resources for acceleration structures.
		// Acceleration structures can only be placed in resources that are created in the default heap (or custom heap equivalent). 
		// Default heap is OK since the application doesn’t need CPU read/write access to them. 
		// The resources that will contain acceleration structures must be created in the state D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, 
		// and must have resource flag D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS. The ALLOW_UNORDERED_ACCESS requirement simply acknowledges both: 
		//  - the system will be doing this type of access in its implementation of acceleration structure builds behind the scenes.
		//  - from the app point of view, synchronization of writes/reads to acceleration structures is accomplished using UAV barriers.
		{
			D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;

			{
				CD3DX12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
				CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(topLevelPrebuildInfo.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
				DIRECTX12_ASSERT(m_Device.Get()->CreateCommittedResource(
					&uploadHeapProperties,
					D3D12_HEAP_FLAG_NONE,
					&bufferDesc,
					initialResourceState,
					nullptr,
					IID_PPV_ARGS(&tlas_DX12->Resource)));
				tlas_DX12->Resource->SetName(L"TopLevelAccelerationStructure");
			}
		}

		// Create an instance desc for the bottom-level acceleration structure.
		ComPtr<ID3D12Resource> instanceDescsResource;
		std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instanceDescs;

		for (u32 i = 0; i < desc.BLASInstances.size(); i++)
		{
			D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};
			BLAS_DX12* blas_DX12 = (BLAS_DX12*)desc.BLASInstances[i].BLAS.Data.get();

			glm::mat4 m = desc.BLASInstances[i].Transform;
			m = glm::transpose(m);
			instanceDesc.Transform[0][0] = m[0][0]; instanceDesc.Transform[0][1] = m[0][1]; instanceDesc.Transform[0][2] = m[0][2]; instanceDesc.Transform[0][3] = m[0][3];
			instanceDesc.Transform[1][0] = m[1][0]; instanceDesc.Transform[1][1] = m[1][1]; instanceDesc.Transform[1][2] = m[1][2]; instanceDesc.Transform[1][3] = m[1][3];
			instanceDesc.Transform[2][0] = m[2][0]; instanceDesc.Transform[2][1] = m[2][1]; instanceDesc.Transform[2][2] = m[2][2]; instanceDesc.Transform[2][3] = m[2][3];
			instanceDesc.InstanceMask = 1;
			instanceDesc.AccelerationStructure = blas_DX12->Resource->GetGPUVirtualAddress();
			instanceDescs.push_back(instanceDesc);
		}

		{
			u64 datasize = static_cast<u64>(instanceDescs.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC));
			CD3DX12_HEAP_PROPERTIES uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
			CD3DX12_RESOURCE_DESC bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(datasize);
			DIRECTX12_ASSERT(m_Device.Get()->CreateCommittedResource(
				&uploadHeapProperties,
				D3D12_HEAP_FLAG_NONE,
				&bufferDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&instanceDescsResource)));
			instanceDescsResource->SetName(L"InstanceDescs");
			void* pMappedData;
			instanceDescsResource->Map(0, nullptr, &pMappedData);
			memcpy(pMappedData, &instanceDescs[0], datasize);
			instanceDescsResource->Unmap(0, nullptr);
		}

		// Top Level Acceleration Structure desc
		D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = {};
		{
			topLevelInputs.InstanceDescs = instanceDescsResource->GetGPUVirtualAddress();
			topLevelBuildDesc.Inputs = topLevelInputs;
			topLevelBuildDesc.DestAccelerationStructureData = tlas_DX12->Resource->GetGPUVirtualAddress();
			topLevelBuildDesc.ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress();
		}

		auto BuildAccelerationStructure = [&](auto* raytracingCommandList)
		{
			raytracingCommandList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);
		};

		// Build acceleration structure.
		BuildAccelerationStructure(commandBuffer->CommandList.Get());

		// Kick off acceleration structure construction.
		ExecuteGraphicsCommandBuffer(commandBuffer);
		Flush();

		TLAS tlas;
		tlas.Data = tlas_DX12;

		return tlas;
	}

	void Device_DX12::UploadTextureData(Ref<Texture> texture, void* Data, u32 mip, u32 slice)
	{
		Texture_DX12* texture_DX12 = (Texture_DX12*)texture->Data.get();

		D3D12_SUBRESOURCE_DATA subresourceData = {};
		subresourceData.pData = Data;
		subresourceData.RowPitch = static_cast<LONG_PTR>(texture->Desc.Width * FormatSizeInBytes(texture->Desc.Format));
		subresourceData.SlicePitch = static_cast<LONG_PTR>(texture->Desc.Width * texture->Desc.Height * FormatSizeInBytes(texture->Desc.Format));

		u32 subResource = D3D12CalcSubresource(mip, slice, 0, texture->Desc.NumMips, texture->Desc.NumArraySlices);

		if (texture_DX12->TextureResource->CurrentState != D3D12_RESOURCE_STATE_COMMON)
		{

			Ref<CommandBuffer> commandList = GetFreeGraphicsCommandBuffer();

			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				texture_DX12->TextureResource->Resource.Get(),
				texture_DX12->TextureResource->CurrentState,
				D3D12_RESOURCE_STATE_COMMON,
				subResource
			);

			commandList->CommandList->ResourceBarrier(1, &barrier);
			
			ExecuteGraphicsCommandBuffer(commandList);
			commandList->Wait(m_FenceEvent);
		}

		CopyToResource(
			texture_DX12->TextureResource->Resource,
			subresourceData,
			subResource
		);

		if (texture_DX12->TextureResource->CurrentState != D3D12_RESOURCE_STATE_COMMON)
		{

			Ref<CommandBuffer> commandList = GetFreeGraphicsCommandBuffer();

			CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
				texture_DX12->TextureResource->Resource.Get(),
				D3D12_RESOURCE_STATE_COMMON,
				texture_DX12->TextureResource->CurrentState,
				subResource
			);

			commandList->CommandList->ResourceBarrier(1, &barrier);

			ExecuteGraphicsCommandBuffer(commandList);
			commandList->Wait(m_FenceEvent);
		}
	}

	bool Device_DX12::Destroy()
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

		return true;
	}

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

	void Device_DX12::DrawTextureInImGui(Ref<Texture> texture, u32 width, u32 height)
	{
		Texture_DX12* texture_DX12 = (Texture_DX12*)texture->Data.get();

		ImVec2 size = {
			static_cast<float>(width), 
			static_cast<float>(height)
		};
		if (width == 0 || height == 0)
		{
			size = { 
				static_cast<float>(texture->Desc.Width), 
				static_cast<float>(texture->Desc.Height) 
			};
		}
		
		for (u32 i = 0; i < texture->Desc.NumMips; i++)
		{
			ImGui::Image(reinterpret_cast<void*>(texture_DX12->mipSrvs[i].GPU.ptr), size);
		}

	};

	void Device_DX12::DrawRenderTargetInImGui(Ref<RenderTarget> renderTarget, u32 width, u32 height, RenderTargetType type)
	{
		RenderTarget_DX12* renderTarget_DX12 = (RenderTarget_DX12*)renderTarget->Data.get();
		ImVec2 size = {
			static_cast<float>(width),
			static_cast<float>(height)
		};
		if (width == 0 || height == 0)
		{
			size = {
				static_cast<float>(renderTarget->Desc.Width),
				static_cast<float>(renderTarget->Desc.Height)
			};
		}

		if (type == RenderTargetType::TargetDepth)
		{
			ImGui::Image(reinterpret_cast<void*>(renderTarget_DX12->depthStencilSrv.GPU.ptr), size);
			return;
		}

		u32 index = 0;

		switch (type)
		{
		case RenderTargetType::Target0: index = 0; break;
		case RenderTargetType::Target1: index = 1; break;
		case RenderTargetType::Target2: index = 2; break;
		case RenderTargetType::Target3: index = 3; break;
		case RenderTargetType::Target4: index = 4; break;
		case RenderTargetType::Target5: index = 5; break;
		case RenderTargetType::Target6: index = 6; break;
		case RenderTargetType::Target7: index = 7; break;
		case RenderTargetType::Target8: index = 8; break;
		default: break;
		}

		ImGui::Image(reinterpret_cast<void*>(renderTarget_DX12->renderTargetSrvs[index].GPU.ptr), size);

	}

	void Device_DX12::SetDeferredReleasesFlag()
	{
		//m_CommandFrames[m_CurrentBackBufferIndex].DeferredReleasesFlag = 1;
	}

	bool IsDirectXRaytracingSupported(IDXGIAdapter1* adapter)
	{
		ComPtr<ID3D12Device> testDevice;
		D3D12_FEATURE_DATA_D3D12_OPTIONS5 featureSupportData = {};

		return SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&testDevice)))
			&& SUCCEEDED(testDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &featureSupportData, sizeof(featureSupportData)))
			&& featureSupportData.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
	}

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

		D3D_FEATURE_LEVEL maxFeatureLevel{ GetMaxFeatureLevel(adapter.Get()) };
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

		DIRECTX12_ASSERT_MSG(IsDirectXRaytracingSupported(adapter.Get()),
			"ERROR: DirectX Raytracing is not supported by your OS, GPU and/or driver.");

	}

	ComPtr<IDXGIAdapter4> Device_DX12::GetAdapter(ComPtr<IDXGIFactory6> factory)
	{
		HRESULT hr{ S_OK };

		ComPtr<IDXGIAdapter4> adapter{ nullptr };

		LOG_DEBUG("Going trhough adapters: ");
		for (u32 i = 0; factory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&adapter)) != DXGI_ERROR_NOT_FOUND; i++)
		{
			DXGI_ADAPTER_DESC1 adapterDesc{};
			adapter->GetDesc1(&adapterDesc);

			hr = D3D12CreateDevice(adapter.Get(), c_MinimumFeatureLevel, __uuidof(ID3D12Device1), nullptr);
			if (SUCCEEDED(hr))
			{
				return adapter;
			}
		}

		ASSERT_MSG(false, "Could not find a suiting adapter!");
		return nullptr;
	}

	D3D_FEATURE_LEVEL Device_DX12::GetMaxFeatureLevel(IDXGIAdapter1* adapter)
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
		DIRECTX12_ASSERT(D3D12CreateDevice(adapter, c_MinimumFeatureLevel, IID_PPV_ARGS(&device)));
		DIRECTX12_ASSERT(device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &featureLevelInfo, sizeof(featureLevelInfo)));
		return featureLevelInfo.MaxSupportedFeatureLevel;
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
			index = (index+1) % m_GraphicsCommandBuffers.size();
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

	void __declspec(noinline) Device_DX12::ProcessDeferredReleases()
	{
		std::lock_guard<std::mutex> lock(m_DeferredReleasesMutex);

		m_RtvDescHeap.ProcessDeferredFree();
		m_DsvDescHeap.ProcessDeferredFree();
		m_SrvDescHeap.ProcessDeferredFree();
		m_UavDescHeap.ProcessDeferredFree();
	}

	template<typename T>
	void CreateCommandBuffers(ComPtr<ID3D12Device8> device, ComPtr<ID3D12CommandQueue>& commandQueue, T& commandBuffers, D3D12_COMMAND_LIST_TYPE type)
	{
		D3D12_COMMAND_QUEUE_DESC queue_desc{};
		queue_desc.Type = type;
		queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
		queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queue_desc.NodeMask = 0;

		DIRECTX12_ASSERT(device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&commandQueue)));

		NAME_DIRECTX12_OBJECT(
			commandQueue,
			type == D3D12_COMMAND_LIST_TYPE_DIRECT ? "Graphics Command Queue" :
			type == D3D12_COMMAND_LIST_TYPE_COPY ? "Copy Command Queue" :
			type == D3D12_COMMAND_LIST_TYPE_COMPUTE ? "Compute Command Queue" :
			"Unkown Command Queue"
		);

		for (u32 i = 0; i < commandBuffers.size(); i++)
		{
			commandBuffers[i] = CreateRef<CommandBuffer>();
			Ref<CommandBuffer> commandBuffer = commandBuffers[i];

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
				commandBuffer->Fence, i,
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

	void Device_DX12::CopyToResource(ComPtr<ID3D12Resource>& resource, D3D12_SUBRESOURCE_DATA& subResourceData,  u32 subResource)
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

	void Device_DX12::Resize(u32 width, u32 height)
	{
		Flush();
		LOG_INFO("Resizeing to: %u, %u", width, height);

		if (width == 0) width = 1;
		if (height == 0) height = 1;

		m_BackBuffers.clear();
		ProcessDeferredReleases();
		Flush();

		DXGI_SWAP_CHAIN_DESC1 desc{};
		m_SwapChain->GetDesc1(&desc);
		m_SwapChain->ResizeBuffers(m_NumBackBuffers, width, height, desc.Format, desc.Flags);


		m_BackBuffers.resize(m_NumBackBuffers);

		for (u32 i = 0; i < m_NumBackBuffers; i++)
		{

			Ref<RenderTarget_DX12> data = CreateRef<RenderTarget_DX12>();
			data->RenderTargetResources[0] = CreateRef<Resource>();

			DIRECTX12_ASSERT(m_SwapChain->GetBuffer(i, IID_PPV_ARGS(&data->RenderTargetResources[0]->Resource)));

			RenderTargetDesc renderTargetDesc;
			renderTargetDesc.RenderTargetFormats[0] = m_BackBufferFormat;
			renderTargetDesc.NumRenderTargets = 1;
			renderTargetDesc.DepthStencilFormat = Format::R32_FLOAT;
			renderTargetDesc.Width = width;
			renderTargetDesc.Height = height;
			renderTargetDesc.RenderTargetClearValues[0].Values[0] = 0.5f;
			renderTargetDesc.RenderTargetClearValues[0].Values[1] = 0.0f;
			renderTargetDesc.RenderTargetClearValues[0].Values[2] = 0.5f;
			renderTargetDesc.RenderTargetClearValues[0].Values[3] = 1.0f;
			renderTargetDesc.DepthTargetClearValue.Depth = 1.0f;

			m_BackBuffers[i] = CreateRenderTarget(renderTargetDesc, data);

		}

		m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();
	}

	inline void AllocateUAVBuffer(ID3D12Device* pDevice, UINT64 bufferSize, ID3D12Resource** ppResource, D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_COMMON, const wchar_t* resourceName = nullptr)
	{
		auto uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
		auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
		DIRECTX12_ASSERT(pDevice->CreateCommittedResource(
			&uploadHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			initialResourceState,
			nullptr,
			IID_PPV_ARGS(ppResource)));
		if (resourceName)
		{
			(*ppResource)->SetName(resourceName);
		}
	}

	inline void AllocateUploadBuffer(ID3D12Device* pDevice, void* pData, UINT64 datasize, ID3D12Resource** ppResource, const wchar_t* resourceName = nullptr)
	{
		auto uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(datasize);
		DIRECTX12_ASSERT(pDevice->CreateCommittedResource(
			&uploadHeapProperties,
			D3D12_HEAP_FLAG_NONE,
			&bufferDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(ppResource)));
		if (resourceName)
		{
			(*ppResource)->SetName(resourceName);
		}
		void* pMappedData;
		(*ppResource)->Map(0, nullptr, &pMappedData);
		memcpy(pMappedData, pData, datasize);
		(*ppResource)->Unmap(0, nullptr);
	}

	//ComPtr<ID3D12Resource> Device_DX12::GenerateAccelerationStructure(Ref<VertexBuffer> vertexBuffer, Ref<IndexBuffer> indexBuffer)
	//{
	//	VertexBuffer_DX12* vertexBuffer_DX12 = (VertexBuffer_DX12*)vertexBuffer->Data.get();
	//	IndexBuffer_DX12* indexBuffer_DX12 = (IndexBuffer_DX12*)indexBuffer->Data.get();
	//	
	//	ComPtr<ID3D12Resource> BLASResource;

	//	Ref<CommandBuffer> commandBuffer = GetFreeGraphicsCommandBuffer();
	//		
	//	// Reset the command list for the acceleration structure construction.
	//		
	//	D3D12_RAYTRACING_GEOMETRY_DESC geometryDesc = {};
	//	geometryDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
	//	geometryDesc.Triangles.IndexBuffer = indexBuffer_DX12->IndexBufferResource->Resource->GetGPUVirtualAddress();
	//	geometryDesc.Triangles.IndexCount = indexBuffer->Desc.NumIndices;
	//	geometryDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
	//	geometryDesc.Triangles.Transform3x4 = 0;
	//	geometryDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
	//	geometryDesc.Triangles.VertexCount = vertexBuffer->Desc.NumVertices;
	//	geometryDesc.Triangles.VertexBuffer.StartAddress = vertexBuffer_DX12->VertexBufferResource->Resource->GetGPUVirtualAddress();
	//	geometryDesc.Triangles.VertexBuffer.StrideInBytes = vertexBuffer->Desc.Layout.Stride;
	//		
	//	// Mark the geometry as opaque. 
	//	// PERFORMANCE TIP: mark geometry as opaque whenever applicable as it can enable important ray processing optimizations.
	//	// Note: When rays encounter opaque geometry an any hit shader will not be executed whether it is present or not.
	//	geometryDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
	//		
	//	// Get required sizes for an acceleration structure.
	//	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
	//	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO bottomLevelPrebuildInfo = {};
	//	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS bottomLevelInputs = {};
	//	bottomLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	//	bottomLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
	//	bottomLevelInputs.Flags = buildFlags;
	//	bottomLevelInputs.NumDescs = 1;
	//	bottomLevelInputs.pGeometryDescs = &geometryDesc;
	//	m_Device->GetRaytracingAccelerationStructurePrebuildInfo(&bottomLevelInputs, &bottomLevelPrebuildInfo);
	//	ASSERT(bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);
	//		
	//	ComPtr<ID3D12Resource> scratchResource;
	//	AllocateUAVBuffer(m_Device.Get(), bottomLevelPrebuildInfo.ScratchDataSizeInBytes, &scratchResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");
	//		
	//	// Allocate resources for acceleration structures.
	//	// Acceleration structures can only be placed in resources that are created in the default heap (or custom heap equivalent). 
	//	// Default heap is OK since the application doesn’t need CPU read/write access to them. 
	//	// The resources that will contain acceleration structures must be created in the state D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, 
	//	// and must have resource flag D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS. The ALLOW_UNORDERED_ACCESS requirement simply acknowledges both: 
	//	//  - the system will be doing this type of access in its implementation of acceleration structure builds behind the scenes.
	//	//  - from the app point of view, synchronization of writes/reads to acceleration structures is accomplished using UAV barriers.
	//	{
	//		D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
	//		
	//		AllocateUAVBuffer(m_Device.Get(), bottomLevelPrebuildInfo.ResultDataMaxSizeInBytes, &BLASResource, initialResourceState, L"BottomLevelAccelerationStructure");
	//	}
	//		
	//	// Create an instance desc for the bottom-level acceleration structure.
	//		
	//	// Bottom Level Acceleration Structure desc
	//	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC bottomLevelBuildDesc = {};
	//	{
	//		bottomLevelBuildDesc.Inputs = bottomLevelInputs;
	//		bottomLevelBuildDesc.ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress();
	//		bottomLevelBuildDesc.DestAccelerationStructureData = BLASResource->GetGPUVirtualAddress();
	//	}
	//		
	//	auto BuildAccelerationStructure = [&](auto* raytracingCommandList)
	//	{
	//		raytracingCommandList->BuildRaytracingAccelerationStructure(&bottomLevelBuildDesc, 0, nullptr);
	//		CD3DX12_RESOURCE_BARRIER uavBarrier = CD3DX12_RESOURCE_BARRIER::UAV(BLASResource.Get());
	//		commandBuffer->CommandList->ResourceBarrier(
	//			1,
	//			&uavBarrier
	//		);
	//	};
	//		
	//	// Build acceleration structure.
	//	BuildAccelerationStructure(commandBuffer->CommandList.Get());
	//		
	//	// Kick off acceleration structure construction.
	//	ExecuteGraphicsCommandBuffer(commandBuffer);
	//	Flush();
	//	
	//	return BLASResource;
	//}

	//void Device_DX12::RefitTLAS(TLASRefitDesc refitDesc)
	//{
	//	Ref<CommandBuffer> commandBuffer = GetFreeGraphicsCommandBuffer();

	//	// Get required sizes for an acceleration structure.
	//	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAGS buildFlags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_BUILD;
	//	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS topLevelInputs = {};
	//	topLevelInputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	//	topLevelInputs.Flags = buildFlags;
	//	topLevelInputs.NumDescs = static_cast<u32>(refitDesc.BLASInstances.size());
	//	topLevelInputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

	//	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO topLevelPrebuildInfo = {};
	//	m_Device->GetRaytracingAccelerationStructurePrebuildInfo(&topLevelInputs, &topLevelPrebuildInfo);
	//	ASSERT(topLevelPrebuildInfo.ResultDataMaxSizeInBytes > 0);

	//	ComPtr<ID3D12Resource> scratchResource;
	//	AllocateUAVBuffer(m_Device.Get(), topLevelPrebuildInfo.ScratchDataSizeInBytes, &scratchResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, L"ScratchResource");

	//	// Allocate resources for acceleration structures.
	//	// Acceleration structures can only be placed in resources that are created in the default heap (or custom heap equivalent). 
	//	// Default heap is OK since the application doesn’t need CPU read/write access to them. 
	//	// The resources that will contain acceleration structures must be created in the state D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, 
	//	// and must have resource flag D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS. The ALLOW_UNORDERED_ACCESS requirement simply acknowledges both: 
	//	//  - the system will be doing this type of access in its implementation of acceleration structure builds behind the scenes.
	//	//  - from the app point of view, synchronization of writes/reads to acceleration structures is accomplished using UAV barriers.
	//	{
	//		D3D12_RESOURCE_STATES initialResourceState = D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;

	//		AllocateUAVBuffer(m_Device.Get(), topLevelPrebuildInfo.ResultDataMaxSizeInBytes, &m_topLevelAccelerationStructure, initialResourceState, L"TopLevelAccelerationStructure");
	//	}

	//	// Create an instance desc for the bottom-level acceleration structure.
	//	ComPtr<ID3D12Resource> instanceDescsResource;
	//	std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instanceDescs;

	//	for (u32 i = 0; i < refitDesc.BLASInstances.size(); i++)
	//	{
	//		D3D12_RAYTRACING_INSTANCE_DESC instanceDesc = {};

	//		glm::mat4 m = refitDesc.BLASInstances[i].Transform;
	//		m = glm::transpose(m);
	//		instanceDesc.Transform[0][0] = m[0][0]; instanceDesc.Transform[0][1] = m[0][1]; instanceDesc.Transform[0][2] = m[0][2]; instanceDesc.Transform[0][3] = m[0][3];
	//		instanceDesc.Transform[1][0] = m[1][0]; instanceDesc.Transform[1][1] = m[1][1]; instanceDesc.Transform[1][2] = m[1][2]; instanceDesc.Transform[1][3] = m[1][3];
	//		instanceDesc.Transform[2][0] = m[2][0]; instanceDesc.Transform[2][1] = m[2][1]; instanceDesc.Transform[2][2] = m[2][2]; instanceDesc.Transform[2][3] = m[2][3];
	//		instanceDesc.InstanceMask = 1;
	//		instanceDesc.AccelerationStructure = m_bottomLevelAccelerationStructures[refitDesc.BLASInstances[i].BlasIndex]->GetGPUVirtualAddress();
	//		instanceDescs.push_back(instanceDesc);
	//	}

	//	AllocateUploadBuffer(
	//		m_Device.Get(), 
	//		&instanceDescs[0], 
	//		instanceDescs.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC), 
	//		&instanceDescsResource,
	//		L"InstanceDescs"
	//	);

	//	// Top Level Acceleration Structure desc
	//	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC topLevelBuildDesc = {};
	//	{
	//		topLevelInputs.InstanceDescs = instanceDescsResource->GetGPUVirtualAddress();
	//		topLevelBuildDesc.Inputs = topLevelInputs;
	//		topLevelBuildDesc.DestAccelerationStructureData = m_topLevelAccelerationStructure->GetGPUVirtualAddress();
	//		topLevelBuildDesc.ScratchAccelerationStructureData = scratchResource->GetGPUVirtualAddress();
	//	}

	//	auto BuildAccelerationStructure = [&](auto* raytracingCommandList)
	//	{
	//		raytracingCommandList->BuildRaytracingAccelerationStructure(&topLevelBuildDesc, 0, nullptr);
	//	};

	//	// Build acceleration structure.
	//	BuildAccelerationStructure(commandBuffer->CommandList.Get());

	//	// Kick off acceleration structure construction.
	//	ExecuteGraphicsCommandBuffer(commandBuffer);
	//	Flush();
	//}

	//void Device_DX12::SetupRaytracing()
	//{

	//	// Create the Raytracing root signatures
	//	{
	//		// Global Root Signature
	//		// This is a root signature that is shared across all raytracing shaders invoked during a DispatchRays() call.
	//		{
	//			CD3DX12_DESCRIPTOR_RANGE UAVDescriptorOutput;
	//			UAVDescriptorOutput.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);
	//			CD3DX12_DESCRIPTOR_RANGE UAVDescriptorPositions;
	//			UAVDescriptorPositions.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1);
	//			CD3DX12_DESCRIPTOR_RANGE UAVDescriptorNormals;
	//			UAVDescriptorNormals.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 2);
	//			CD3DX12_DESCRIPTOR_RANGE DescriptorSceneData;
	//			DescriptorSceneData.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
	//			CD3DX12_ROOT_PARAMETER rootParameters[5];
	//			rootParameters[0].InitAsDescriptorTable(1, &UAVDescriptorOutput);
	//			rootParameters[1].InitAsDescriptorTable(1, &UAVDescriptorPositions);
	//			rootParameters[2].InitAsShaderResourceView(0);
	//			rootParameters[3].InitAsDescriptorTable(1, &DescriptorSceneData);
	//			rootParameters[4].InitAsDescriptorTable(1, &UAVDescriptorNormals);
	//			CD3DX12_ROOT_SIGNATURE_DESC globalRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);
	//			
	//			ComPtr<ID3DBlob> blob;
	//			ComPtr<ID3DBlob> error;

	//			DIRECTX12_ASSERT(
	//				D3D12SerializeRootSignature(&globalRootSignatureDesc,
	//					D3D_ROOT_SIGNATURE_VERSION_1, 
	//					&blob, 
	//					&error)
	//			);
	//			DIRECTX12_ASSERT(m_Device->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&(m_raytracingGlobalRootSignature))));
	//		}

	//		// Local Root Signature
	//		// This is a root signature that enables a shader to have unique arguments that come from shader tables.
	//		{
	//			CD3DX12_ROOT_PARAMETER rootParameters[1];
	//			rootParameters[0].InitAsConstants(SizeOfInUint32(m_rayGenCB), 1, 1);
	//			CD3DX12_ROOT_SIGNATURE_DESC localRootSignatureDesc(ARRAYSIZE(rootParameters), rootParameters);
	//			localRootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
	//			
	//			ComPtr<ID3DBlob> blob;
	//			ComPtr<ID3DBlob> error;

	//			DIRECTX12_ASSERT(
	//				D3D12SerializeRootSignature(
	//					&localRootSignatureDesc,
	//					D3D_ROOT_SIGNATURE_VERSION_1, 
	//					&blob, 
	//					&error)
	//			);

	//			DIRECTX12_ASSERT(
	//				m_Device->CreateRootSignature(1, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&(m_raytracingLocalRootSignature))));

	//		}
	//	}

	//	// Create pipeline for raytracing
	//	{
	//		CD3DX12_STATE_OBJECT_DESC raytracingPipeline{ D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE };

	//		// Load the library shader
	//		CD3DX12_DXIL_LIBRARY_SUBOBJECT* lib = raytracingPipeline.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();

	//		std::wstring libSource = L"Shaders/Raytracing.cso";

	//		ComPtr<ID3DBlob> libShaderBlob;
	//		D3DReadFileToBlob(libSource.c_str(), &libShaderBlob);

	//		D3D12_SHADER_BYTECODE libdxil = CD3DX12_SHADER_BYTECODE(libShaderBlob.Get());
	//		lib->SetDXILLibrary(&libdxil);
	//		// Set the different raygen/hit/miss shaders
	//		{
	//			lib->DefineExport(c_raygenShaderName);
	//			lib->DefineExport(c_closestHitShaderName);
	//			lib->DefineExport(c_missShaderName);
	//		}

	//		// TriangleHitGroup
	//		CD3DX12_HIT_GROUP_SUBOBJECT* hitGroup = 
	//			raytracingPipeline.CreateSubobject<CD3DX12_HIT_GROUP_SUBOBJECT>();
	//		hitGroup->SetClosestHitShaderImport(c_closestHitShaderName);
	//		hitGroup->SetHitGroupExport(c_hitGroupName);
	//		hitGroup->SetHitGroupType(D3D12_HIT_GROUP_TYPE_TRIANGLES);

	//		// config
	//		CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT* shaderConfig = 
	//			raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT>();
	//		UINT payloadSize = 4 * sizeof(float);   // float4 color
	//		UINT attributeSize = 2 * sizeof(float); // float2 barycentrics
	//		shaderConfig->Config(payloadSize, attributeSize);

	//		// Create local root signature
	//		{
	//			auto localRootSignature = raytracingPipeline.CreateSubobject<CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT>();
	//			localRootSignature->SetRootSignature(m_raytracingLocalRootSignature.Get());
	//			// Shader association
	//			auto rootSignatureAssociation = raytracingPipeline.CreateSubobject<CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT>();
	//			rootSignatureAssociation->SetSubobjectToAssociate(*localRootSignature);
	//			rootSignatureAssociation->AddExport(c_raygenShaderName);
	//		}

	//		// Create global root signature
	//		auto globalRootSignature = raytracingPipeline.CreateSubobject<CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT>();
	//		globalRootSignature->SetRootSignature(m_raytracingGlobalRootSignature.Get());

	//		// Set pipeline config for max recursion depth
	//		auto pipelineConfig = raytracingPipeline.CreateSubobject<CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT>();
	//		UINT maxRecursionDepth = 1; // ~ primary rays only. 
	//		pipelineConfig->Config(maxRecursionDepth);

	//	
	//		DIRECTX12_ASSERT_MSG(m_Device->CreateStateObject(raytracingPipeline, IID_PPV_ARGS(&m_dxrStateObject)),
	//			"Couldn't create DirectX Raytracing state object.\n");
	//	}


	//	// Build shader tables
	//	
	//	{
	//		void* rayGenShaderIdentifier;
	//		void* missShaderIdentifier;
	//		void* hitGroupShaderIdentifier;

	//		auto GetShaderIdentifiers = [&](auto* stateObjectProperties)
	//		{
	//			rayGenShaderIdentifier = stateObjectProperties->GetShaderIdentifier(c_raygenShaderName);
	//			missShaderIdentifier = stateObjectProperties->GetShaderIdentifier(c_missShaderName);
	//			hitGroupShaderIdentifier = stateObjectProperties->GetShaderIdentifier(c_hitGroupName);
	//		};

	//		// Get shader identifiers.
	//		UINT shaderIdentifierSize;
	//		{
	//			ComPtr<ID3D12StateObjectProperties> stateObjectProperties;
	//			DIRECTX12_ASSERT(m_dxrStateObject.As(&stateObjectProperties));
	//			GetShaderIdentifiers(stateObjectProperties.Get());
	//			shaderIdentifierSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
	//		}
	//		m_rayGenCB.viewport.bottom = 0.;
	//		m_rayGenCB.viewport.top = 1.;
	//		m_rayGenCB.viewport.right = 0.;
	//		m_rayGenCB.viewport.left = 1.;
	//		// Ray gen shader table
	//		{
	//			struct RootArguments {
	//				RayGenConstantBuffer cb;
	//			} rootArguments;
	//			rootArguments.cb = m_rayGenCB;

	//			UINT numShaderRecords = 1;
	//			UINT shaderRecordSize = shaderIdentifierSize + sizeof(rootArguments);
	//			ShaderTable rayGenShaderTable(m_Device.Get(), numShaderRecords, shaderRecordSize, L"RayGenShaderTable");
	//			rayGenShaderTable.push_back(ShaderRecord(rayGenShaderIdentifier, shaderIdentifierSize, &rootArguments, sizeof(rootArguments)));
	//			m_rayGenShaderTable = rayGenShaderTable.GetResource();
	//		}

	//		// Miss shader table
	//		{
	//			UINT numShaderRecords = 1;
	//			UINT shaderRecordSize = shaderIdentifierSize;
	//			ShaderTable missShaderTable(m_Device.Get(), numShaderRecords, shaderRecordSize, L"MissShaderTable");
	//			missShaderTable.push_back(ShaderRecord(missShaderIdentifier, shaderIdentifierSize));
	//			m_missShaderTable = missShaderTable.GetResource();
	//		}

	//		// Hit group shader table
	//		{
	//			UINT numShaderRecords = 1;
	//			UINT shaderRecordSize = shaderIdentifierSize;
	//			ShaderTable hitGroupShaderTable(m_Device.Get(), numShaderRecords, shaderRecordSize, L"HitGroupShaderTable");
	//			hitGroupShaderTable.push_back(ShaderRecord(hitGroupShaderIdentifier, shaderIdentifierSize));
	//			m_hitGroupShaderTable = hitGroupShaderTable.GetResource();
	//		}
	//	}

	//}

	//void Device_DX12::RayTraceRender(Ref<CommandList> commandList, Ref<Texture> target, Ref<RenderTarget> input, RenderTargetType inputPosition, RenderTargetType inputNormal, Ref<ConstantBuffer> sceneData)
	//{
	//	CommandList_DX12& commandListDx12 = *(CommandList_DX12*)commandList.get();
	//	Texture_DX12* texture_DX12 = (Texture_DX12*)target->Data.get();


	//	auto DispatchRays = [&](auto* commandList, auto* stateObject, auto* dispatchDesc)
	//	{
	//		// Since each shader table has only one shader record, the stride is same as the size.
	//		dispatchDesc->HitGroupTable.StartAddress = m_hitGroupShaderTable->GetGPUVirtualAddress();
	//		dispatchDesc->HitGroupTable.SizeInBytes = m_hitGroupShaderTable->GetDesc().Width;
	//		dispatchDesc->HitGroupTable.StrideInBytes = dispatchDesc->HitGroupTable.SizeInBytes;
	//		dispatchDesc->MissShaderTable.StartAddress = m_missShaderTable->GetGPUVirtualAddress();
	//		dispatchDesc->MissShaderTable.SizeInBytes = m_missShaderTable->GetDesc().Width;
	//		dispatchDesc->MissShaderTable.StrideInBytes = dispatchDesc->MissShaderTable.SizeInBytes;
	//		dispatchDesc->RayGenerationShaderRecord.StartAddress = m_rayGenShaderTable->GetGPUVirtualAddress();
	//		dispatchDesc->RayGenerationShaderRecord.SizeInBytes = m_rayGenShaderTable->GetDesc().Width;
	//		dispatchDesc->Width = 1920;
	//		dispatchDesc->Height = 1080;
	//		dispatchDesc->Depth = 1;
	//		commandList->SetPipelineState1(stateObject);
	//		commandList->DispatchRays(dispatchDesc);
	//	};

	//	commandListDx12.CommandBuffer->CommandList->SetComputeRootSignature(m_raytracingGlobalRootSignature.Get());

	//	// Bind the heaps, acceleration structure and dispatch rays.    
	//	D3D12_DISPATCH_RAYS_DESC dispatchDesc = {};
	//	commandListDx12.CommandBuffer->CommandList->SetComputeRootDescriptorTable(0, texture_DX12->uav.GPU);


	//	RenderTarget_DX12* renderTarget_DX12 = (RenderTarget_DX12*)input->Data.get();

	//	DescriptorHandle* descriptorHandlePositions;
	//	DescriptorHandle* descriptorHandleNormal;

	//	switch (inputPosition)
	//	{
	//	case RenderTargetType::Target0: descriptorHandlePositions = &renderTarget_DX12->renderTargetSrvs[0]; break;
	//	case RenderTargetType::Target1: descriptorHandlePositions = &renderTarget_DX12->renderTargetSrvs[1]; break;
	//	case RenderTargetType::Target2: descriptorHandlePositions = &renderTarget_DX12->renderTargetSrvs[2]; break;
	//	case RenderTargetType::Target3: descriptorHandlePositions = &renderTarget_DX12->renderTargetSrvs[3]; break;
	//	case RenderTargetType::Target4: descriptorHandlePositions = &renderTarget_DX12->renderTargetSrvs[4]; break;
	//	case RenderTargetType::Target5: descriptorHandlePositions = &renderTarget_DX12->renderTargetSrvs[5]; break;
	//	case RenderTargetType::Target6: descriptorHandlePositions = &renderTarget_DX12->renderTargetSrvs[6]; break;
	//	case RenderTargetType::Target7: descriptorHandlePositions = &renderTarget_DX12->renderTargetSrvs[7]; break;
	//	case RenderTargetType::Target8: descriptorHandlePositions = &renderTarget_DX12->renderTargetSrvs[8]; break;
	//	case RenderTargetType::TargetDepth: descriptorHandlePositions = &renderTarget_DX12->depthStencilSrv; break;
	//	default:
	//		ASSERT_MSG(false, "Unkown renderTargetType");
	//		return;
	//		break;
	//	}
	//	switch (inputNormal)
	//	{
	//	case RenderTargetType::Target0: descriptorHandleNormal = &renderTarget_DX12->renderTargetSrvs[0]; break;
	//	case RenderTargetType::Target1: descriptorHandleNormal = &renderTarget_DX12->renderTargetSrvs[1]; break;
	//	case RenderTargetType::Target2: descriptorHandleNormal = &renderTarget_DX12->renderTargetSrvs[2]; break;
	//	case RenderTargetType::Target3: descriptorHandleNormal = &renderTarget_DX12->renderTargetSrvs[3]; break;
	//	case RenderTargetType::Target4: descriptorHandleNormal = &renderTarget_DX12->renderTargetSrvs[4]; break;
	//	case RenderTargetType::Target5: descriptorHandleNormal = &renderTarget_DX12->renderTargetSrvs[5]; break;
	//	case RenderTargetType::Target6: descriptorHandleNormal = &renderTarget_DX12->renderTargetSrvs[6]; break;
	//	case RenderTargetType::Target7: descriptorHandleNormal = &renderTarget_DX12->renderTargetSrvs[7]; break;
	//	case RenderTargetType::Target8: descriptorHandleNormal = &renderTarget_DX12->renderTargetSrvs[8]; break;
	//	case RenderTargetType::TargetDepth: descriptorHandleNormal = &renderTarget_DX12->depthStencilSrv; break;
	//	default:
	//		ASSERT_MSG(false, "Unkown renderTargetType");
	//		return;
	//		break;
	//	}
	//	
	//	commandListDx12.TransitionRenderTarget(input, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);

	//	commandListDx12.CommandBuffer->CommandList->SetComputeRootDescriptorTable(1, descriptorHandlePositions->GPU);
	//	commandListDx12.CommandBuffer->CommandList->SetComputeRootDescriptorTable(4, descriptorHandleNormal->GPU);
	//	
	//	
	//	commandListDx12.CommandBuffer->CommandList->SetComputeRootShaderResourceView(
	//		2, m_topLevelAccelerationStructure->GetGPUVirtualAddress()
	//	);

	//	ConstantBuffer_DX12* constantBuffer_DX12 = (ConstantBuffer_DX12*)sceneData->Data.get();
	//	commandListDx12.CommandBuffer->CommandList->SetComputeRootDescriptorTable(
	//		3, constantBuffer_DX12->cbv.GPU
	//	);
	//	
	//	
	//	DispatchRays(commandListDx12.CommandBuffer->CommandList.Get(), m_dxrStateObject.Get(), &dispatchDesc);

	//}

	//void Device_DX12::ShutdownRaytracing()
	//{
	//}


} }

#endif // WIN32
