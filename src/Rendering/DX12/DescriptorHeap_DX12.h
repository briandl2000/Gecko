#ifdef DIRECTX_12
#pragma once

#include <mutex>
#include <vector>

#include "Rendering/DX12/CommonHeaders_DX12.h"

namespace Gecko::DX12
{

  class Device_DX12;
  class DescriptorHeap;

  struct DescriptorHandle
  {
    D3D12_CPU_DESCRIPTOR_HANDLE CPU{};
    D3D12_GPU_DESCRIPTOR_HANDLE GPU{};

    constexpr bool IsValid() const { return CPU.ptr != 0; };
    constexpr bool IsShaderVisible() const { return GPU.ptr != 0; };

#ifdef DEBUG
  private:
    friend class DescriptorHeap;
    DescriptorHeap* Container{ nullptr };
    u32 Index{};
#endif
  };

  class DescriptorHeap
  {
  public:
    explicit DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type) : m_Type(type) {}
    DISABLE_COPY_AND_MOVE(DescriptorHeap);
    ~DescriptorHeap() {};

    bool Initialize(Device_DX12* device, u32 capacity, bool isShaderVisible);
    void Destroy();
    void ProcessDeferredFree();

    [[nodiscard]] DescriptorHandle Allocate();
    void Free(DescriptorHandle& handle);

    D3D12_DESCRIPTOR_HEAP_TYPE Type() const { return m_Type; };
    D3D12_CPU_DESCRIPTOR_HANDLE CPUStart() const { return m_CPUStart; };
    D3D12_GPU_DESCRIPTOR_HANDLE GPUStart() const { return m_GPUStart; };
    ID3D12DescriptorHeap* Heap() const { return m_Heap.Get(); };
    u32 Capacity() const { return m_Capacity; };
    u32 Size() const { return m_Size; };
    u32 DescriptorSize() const { return m_DescriptorSize; };
    bool IsShaderVisible() const { return m_GPUStart.ptr != 0; };

  private:
    ComPtr<ID3D12DescriptorHeap> m_Heap = nullptr;

    D3D12_CPU_DESCRIPTOR_HANDLE m_CPUStart{ 0 };
    D3D12_GPU_DESCRIPTOR_HANDLE m_GPUStart{ 0 };

    std::mutex m_Mutex;

    u32 m_Capacity = 0;
    u32 m_Size = 0;
    u32 m_DescriptorSize = 0;
    const D3D12_DESCRIPTOR_HEAP_TYPE m_Type;

    Scope<u32[]> m_FreeHandles;
    std::vector<u32> m_DeferredFreeIndices;

    Device_DX12* m_Device;
  };

}

#endif // WIN32
