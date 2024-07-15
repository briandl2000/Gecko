#pragma once
#ifdef WIN32

#include <vector>
#include <array>

#include "CommonHeaders_DX12.h"

#include "Objects_DX12.h"
#include "Rendering/Backend/Device.h"
#include "Resources_DX12.h"

namespace Gecko { namespace DX12
{


	class GpuUploadBuffer
	{
	public:
		ComPtr<ID3D12Resource> GetResource() { return m_resource; }

	protected:
		ComPtr<ID3D12Resource> m_resource;

		GpuUploadBuffer() {}
		~GpuUploadBuffer()
		{
			if (m_resource.Get())
			{
				m_resource->Unmap(0, nullptr);
			}
		}

		void Allocate(ID3D12Device* device, UINT bufferSize, LPCWSTR resourceName = nullptr)
		{
			auto uploadHeapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);

			auto bufferDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);
			DIRECTX12_ASSERT(device->CreateCommittedResource(
				&uploadHeapProperties,
				D3D12_HEAP_FLAG_NONE,
				&bufferDesc,
				D3D12_RESOURCE_STATE_GENERIC_READ,
				nullptr,
				IID_PPV_ARGS(&m_resource)));
			m_resource->SetName(resourceName);
		}

		uint8_t* MapCpuWriteOnly()
		{
			uint8_t* mappedData;
			// We don't unmap this until the app closes. Keeping buffer mapped for the lifetime of the resource is okay.
			CD3DX12_RANGE readRange(0, 0);        // We do not intend to read from this resource on the CPU.
			DIRECTX12_ASSERT(m_resource->Map(0, &readRange, reinterpret_cast<void**>(&mappedData)));
			return mappedData;
		}
	};
	inline UINT Align(UINT size, UINT alignment)
	{
		return (size + (alignment - 1)) & ~(alignment - 1);
	}

	class ShaderRecord
	{
	public:
		ShaderRecord(void* pShaderIdentifier, UINT shaderIdentifierSize) :
			shaderIdentifier(pShaderIdentifier, shaderIdentifierSize)
		{
		}

		ShaderRecord(void* pShaderIdentifier, UINT shaderIdentifierSize, void* pLocalRootArguments, UINT localRootArgumentsSize) :
			shaderIdentifier(pShaderIdentifier, shaderIdentifierSize),
			localRootArguments(pLocalRootArguments, localRootArgumentsSize)
		{
		}

		void CopyTo(void* dest) const
		{
			uint8_t* byteDest = static_cast<uint8_t*>(dest);
			memcpy(byteDest, shaderIdentifier.ptr, shaderIdentifier.size);
			if (localRootArguments.ptr)
			{
				memcpy(byteDest + shaderIdentifier.size, localRootArguments.ptr, localRootArguments.size);
			}
		}

		struct PointerWithSize {
			void* ptr;
			UINT size;

			PointerWithSize() : ptr(nullptr), size(0) {}
			PointerWithSize(void* _ptr, UINT _size) : ptr(_ptr), size(_size) {};
		};
		PointerWithSize shaderIdentifier;
		PointerWithSize localRootArguments;
	};

	class ShaderTable : public GpuUploadBuffer
	{
		uint8_t* m_mappedShaderRecords{nullptr};
		UINT m_shaderRecordSize{0};

		// Debug support
		std::wstring m_name;
		std::vector<ShaderRecord> m_shaderRecords;

		ShaderTable() {}
	public:
		ShaderTable(ID3D12Device* device, UINT numShaderRecords, UINT shaderRecordSize, LPCWSTR resourceName = nullptr)
			: m_name(resourceName)
		{
			m_shaderRecordSize = Align(shaderRecordSize, D3D12_RAYTRACING_SHADER_RECORD_BYTE_ALIGNMENT);
			m_shaderRecords.reserve(numShaderRecords);
			UINT bufferSize = numShaderRecords * m_shaderRecordSize;
			Allocate(device, bufferSize, resourceName);
			m_mappedShaderRecords = MapCpuWriteOnly();
		}

		void push_back(const ShaderRecord& shaderRecord)
		{
			ASSERT(m_shaderRecords.size() < m_shaderRecords.capacity());
			m_shaderRecords.push_back(shaderRecord);
			shaderRecord.CopyTo(m_mappedShaderRecords);
			m_mappedShaderRecords += m_shaderRecordSize;
		}

		UINT GetShaderRecordSize() { return m_shaderRecordSize; }
	};

	struct Viewport
	{
		float left;
		float top;
		float right;
		float bottom;
	};

	struct RayGenConstantBuffer
	{
		Viewport viewport;
	};


	constexpr D3D_FEATURE_LEVEL c_MinimumFeatureLevel{ D3D_FEATURE_LEVEL_11_0 };

	struct RenderTarget_DX12;

	class Device_DX12 : public Device
	{
	public:

		Device_DX12();
		virtual ~Device_DX12() override;

		virtual Ref<CommandList> CreateGraphicsCommandList() override;
		virtual void ExecuteGraphicsCommandList(Ref<CommandList> commandList) override;
		virtual void ExecuteGraphicsCommandListAndFlip(Ref<CommandList> commandList) override;

		virtual Ref<CommandList> CreateComputeCommandList() override;
		virtual void ExecuteComputeCommandList(Ref<CommandList> commandList) override;

		virtual RenderTarget GetCurrentBackBuffer() override;

		virtual RenderTarget CreateRenderTarget(const RenderTargetDesc& desc) override;
		virtual VertexBuffer CreateVertexBuffer(const VertexBufferDesc& desc) override;
		virtual IndexBuffer CreateIndexBuffer(const IndexBufferDesc& desc) override;
		virtual GraphicsPipeline CreateGraphicsPipeline(const GraphicsPipelineDesc& desc) override;
		virtual ComputePipeline CreateComputePipeline(const ComputePipelineDesc& desc) override;
		virtual Texture CreateTexture(const TextureDesc& desc) override;
		virtual ConstantBuffer CreateConstantBuffer(const ConstantBufferDesc& desc) override;

		// Raytracing
		virtual RaytracingPipeline CreateRaytracingPipeline(const RaytracingPipelineDesc& desc) override;
		virtual BLAS CreateBLAS(const BLASDesc& desc) override;
		virtual TLAS CreateTLAS(const TLASRefitDesc& desc) override;

		virtual void UploadTextureData(Texture texture, void* Data, u32 mip = 0, u32 slice = 0) override;

		virtual void ImGuiRender(Ref<CommandList> commandList) override;
		virtual void DrawRenderTargetInImGui(RenderTarget renderTarget, u32 width = 0, u32 height = 0, RenderTargetType type = RenderTargetType::Target0) override;
		virtual void DrawTextureInImGui(Texture texture, u32 width = 0, u32 height = 0) override;

		virtual bool Destroy() override;

		virtual u32 GetNumBackBuffers() { return m_NumBackBuffers; };
		virtual u32 GetCurrentBackBufferIndex() { return m_CurrentBackBufferIndex; };

		const ComPtr<ID3D12Device8> GetDevice() { return m_Device; };

		void SetDeferredReleasesFlag();

		u32 GetBackBufferCount() { return m_NumBackBuffers; }

		void Flush();

		DescriptorHeap& GetRtvHeap() { return m_RtvDescHeap; }
		DescriptorHeap& GetDsvHeap() { return m_DsvDescHeap; }
		DescriptorHeap& GetSrvHeap() { return m_SrvDescHeap; }
		DescriptorHeap& GetUavHeap() { return m_UavDescHeap; }

		Ref<CommandBuffer> GetFreeGraphicsCommandBuffer();
		Ref<CommandBuffer> GetFreeComputeCommandBuffer();
		Ref<CommandBuffer> GetFreeCopyCommandBuffer();

		void ExecuteGraphicsCommandBuffer(Ref<CommandBuffer> copyCommandBuffer);
		void ExecuteComputeCommandBuffer(Ref<CommandBuffer> copyCommandBuffer);
		void ExecuteCopyCommandBuffer(Ref<CommandBuffer> copyCommandBuffer);

		void Resize(u32 width, u32 height);

		/*void SetupRaytracing();
		ComPtr<ID3D12Resource> GenerateAccelerationStructure(VertexBuffer, IndexBuffer);
		void RefitTLAS(TLASRefitDesc refitDesc);
		void RayTraceRender(Ref<CommandList> commandList, Texture target, RenderTarget input, RenderTargetType inputPosition, RenderTargetType inputNormal, ConstantBuffer sceneData);
		void ShutdownRaytracing();*/

	private:

		RenderTarget Device_DX12::CreateRenderTarget(const RenderTargetDesc& desc, Ref<RenderTarget_DX12>& renderTargetDX12);

		void CopyToResource(ComPtr<ID3D12Resource>& resource, D3D12_SUBRESOURCE_DATA& subResourceData, u32 subResource = 0);

		void CreateDevice();

		void ProcessDeferredReleases();

		ComPtr<IDXGIAdapter4> GetAdapter(ComPtr<IDXGIFactory6> factory);

		D3D_FEATURE_LEVEL GetMaxFeatureLevel(IDXGIAdapter1* adapter);

		// Device Data
		ComPtr<ID3D12Device8> m_Device;
		ComPtr<IDXGIFactory7> m_Factory;

		// Heaps
		DescriptorHeap m_RtvDescHeap{ D3D12_DESCRIPTOR_HEAP_TYPE_RTV };
		DescriptorHeap m_DsvDescHeap{ D3D12_DESCRIPTOR_HEAP_TYPE_DSV };
		DescriptorHeap m_SrvDescHeap{ D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV };
		DescriptorHeap m_UavDescHeap{ D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV };

		DescriptorHandle imGuiHandle;

		std::mutex m_DeferredReleasesMutex;
		// Back buffers

		std::vector<RenderTarget> m_BackBuffers;

		ComPtr<IDXGISwapChain4> m_SwapChain;

		u32 m_NumBackBuffers;

		static constexpr u32 c_NumGraphicsCommandBuffers = 1;
		static constexpr u32 c_NumComputeCommandBuffers = 1;
		static constexpr u32 c_NumCopyCommandBuffers = 1;

		std::array<Ref<CommandBuffer>, c_NumGraphicsCommandBuffers> m_GraphicsCommandBuffers;
		std::array<Ref<CommandBuffer>, c_NumComputeCommandBuffers> m_ComputeCommandBuffers;
		std::array<Ref<CommandBuffer>, c_NumCopyCommandBuffers> m_CopyCommandBuffers;

		HANDLE m_FenceEvent{ nullptr };

		ComPtr<ID3D12CommandQueue> m_GraphicsCommandQueue{ nullptr };
		ComPtr<ID3D12CommandQueue> m_ComputeCommandQueue{ nullptr };
		ComPtr<ID3D12CommandQueue> m_CopyCommandQueue{ nullptr };

		u32 m_CurrentBackBufferIndex;

		Format m_BackBufferFormat;

		bool SupportsRaytracing{ false };

		// Raytracing info
		//ComPtr<ID3D12RootSignature> m_raytracingLocalRootSignature;
		//ComPtr<ID3D12RootSignature> m_raytracingGlobalRootSignature;
		//ComPtr<ID3D12StateObject> m_dxrStateObject;

		//RayGenConstantBuffer m_rayGenCB;

		//ComPtr<ID3D12Resource> m_accelerationStructure;
		//std::vector<ComPtr<ID3D12Resource>> m_bottomLevelAccelerationStructures;
		//ComPtr<ID3D12Resource> m_topLevelAccelerationStructure;

		//ComPtr<ID3D12Resource> m_missShaderTable;
		//ComPtr<ID3D12Resource> m_hitGroupShaderTable;
		//ComPtr<ID3D12Resource> m_rayGenShaderTable;
	};

} }
#endif