#ifdef DIRECTX_12

#include "Rendering/DX12/DescriptorHeap_DX12.h"
#include "Rendering/DX12/Device_DX12.h"

namespace Gecko { namespace DX12 {

    bool DescriptorHeap::Initialize(Device_DX12* device, u32 capacity, bool isShaderVisible)
    {
        m_Device = device;
        //m_DeferredFreeIndices.resize(device->GetBackBufferCount());

        std::lock_guard<std::mutex> lock(m_Mutex);

        ASSERT(capacity && capacity < D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_2);
        ASSERT(!(m_Type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER && capacity > D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE));

        if (m_Type == D3D12_DESCRIPTOR_HEAP_TYPE_DSV || m_Type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV)
        {
            isShaderVisible = false;
        }

        ASSERT(m_Device->GetDevice());

        D3D12_DESCRIPTOR_HEAP_DESC desc{};
        desc.Type = m_Type;
        desc.NumDescriptors = capacity;
        desc.Flags = isShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        desc.NodeMask = 0;

        DIRECTX12_ASSERT(m_Device->GetDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_Heap)));

        m_FreeHandles = std::move(CreateScope<u32[]>(capacity));
        m_Capacity = capacity;
        m_Size = 0;

        for (u32 i = 0; i < capacity; i++) m_FreeHandles[i] = i;

        m_DescriptorSize = m_Device->GetDevice()->GetDescriptorHandleIncrementSize(m_Type);
        m_CPUStart = m_Heap->GetCPUDescriptorHandleForHeapStart();
        m_GPUStart = isShaderVisible ? m_Heap->GetGPUDescriptorHandleForHeapStart() : D3D12_GPU_DESCRIPTOR_HANDLE{ 0 };

        return true;
    }

    void DescriptorHeap::Destroy()
    {
        m_Heap = nullptr;
    }

    void DescriptorHeap::ProcessDeferredFree()
    {
        std::lock_guard<std::mutex> lock(m_Mutex);

        if (!m_DeferredFreeIndices.empty())
        {
            for (auto index : m_DeferredFreeIndices)
            {
                m_Size--;
                m_FreeHandles[m_Size] = index;
            }
            m_DeferredFreeIndices.clear();
        }
    }

    [[nodiscard]] DescriptorHandle DescriptorHeap::Allocate()
    {
        std::lock_guard<std::mutex> lock{ m_Mutex };

        ASSERT(m_Heap);
        ASSERT(m_Size < m_Capacity);

        const u32 index{ m_FreeHandles[m_Size] };
        const u32 offset{ index * m_DescriptorSize };
        m_Size++;

        DescriptorHandle handle;
        handle.CPU.ptr = m_CPUStart.ptr + offset;

        if (IsShaderVisible())
            handle.GPU.ptr = m_GPUStart.ptr + offset;

#ifdef DEBUG
        handle.Container = this;
        handle.Index = index;
#endif

        return handle;
    }

    void DescriptorHeap::Free(DescriptorHandle& handle)
    {
        if (!handle.IsValid()) return;
        std::lock_guard<std::mutex> lock{ m_Mutex };
        ASSERT(m_Heap && m_Size);
        ASSERT(handle.CPU.ptr >= m_CPUStart.ptr);
        ASSERT((handle.CPU.ptr - m_CPUStart.ptr) % m_DescriptorSize == 0);
        const u32 index{ static_cast<u32>(handle.CPU.ptr - m_CPUStart.ptr) / m_DescriptorSize };
#ifdef DEBUG
        ASSERT(handle.Container == this);
        ASSERT(handle.Index < m_Capacity);
        ASSERT(handle.Index == index);
#endif

        m_DeferredFreeIndices.push_back(index);
        m_Device->SetDeferredReleasesFlag();

        handle = {};
    }
} }

#endif