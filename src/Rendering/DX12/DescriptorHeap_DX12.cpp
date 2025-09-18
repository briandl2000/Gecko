#ifdef DIRECTX_12

#include "Rendering/DX12/DescriptorHeap_DX12.h"
#include "Rendering/DX12/Device_DX12.h"

namespace Gecko::DX12
{

  bool DescriptorHeap::Initialize(Device_DX12* device, u32 capacity, bool isShaderVisible)
  {
    m_Device = device;
    //m_DeferredFreeIndices.resize(device->GetBackBufferCount());

    std::lock_guard<std::mutex> lock(m_Mutex);

    ASSERT(capacity, "Cannot create a descriptor heap with no capacity!")
      ASSERT(capacity < D3D12_MAX_SHADER_VISIBLE_DESCRIPTOR_HEAP_SIZE_TIER_2, "Requested capacity for descriptor heap exceeds max capacity!");
    ASSERT(!(m_Type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER && capacity > D3D12_MAX_SHADER_VISIBLE_SAMPLER_HEAP_SIZE),
      "Requested capacity for sampler heap exceeds max capacity!");

    if (m_Type == D3D12_DESCRIPTOR_HEAP_TYPE_DSV || m_Type == D3D12_DESCRIPTOR_HEAP_TYPE_RTV)
    {
      isShaderVisible = false;
    }

    ASSERT(m_Device->GetDevice(), "Invalid device!");

    D3D12_DESCRIPTOR_HEAP_DESC desc{};
    desc.Type = m_Type;
    desc.NumDescriptors = capacity;
    desc.Flags = isShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    desc.NodeMask = 0;

    DIRECTX12_ASSERT(m_Device->GetDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_Heap)));

    m_FreeHandles = CreateScope<u32[]>(capacity);
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

    ASSERT(m_Heap, "Invalid heap, cannot allocate!");
    ASSERT(m_Size < m_Capacity, "Attempting to allocate more than capacity!");

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
    ASSERT(m_Heap && m_Size, "Attempting to free an empty heap!");
    ASSERT(handle.CPU.ptr >= m_CPUStart.ptr, "Invalid CPU pointer to heap!");
    ASSERT((handle.CPU.ptr - m_CPUStart.ptr) % m_DescriptorSize == 0, "Invalid offset into heap!");
    const u32 index{ static_cast<u32>(handle.CPU.ptr - m_CPUStart.ptr) / m_DescriptorSize };
#ifdef DEBUG
    ASSERT(handle.Container == this, "Mismatching heap pointer between heap and handle!");
    ASSERT(handle.Index < m_Capacity, "Attempting to free out of bounds!");
    ASSERT(handle.Index == index, "Mismatching memory offset between heap and handle!");
#endif

    m_DeferredFreeIndices.push_back(index);

    handle = {};
  }
}

#endif
