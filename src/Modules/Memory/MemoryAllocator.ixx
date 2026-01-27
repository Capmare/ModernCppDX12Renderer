//
// Created by capma on 26-Jan-26.
//

module;
#include <d3d12.h>
#include <dxgi1_6.h>
#include "../../ThirdParty/D3D12MA/D3D12MemAlloc.h"

export module HOX.MemoryAllocator;
import std;
import HOX.Win32;
import HOX.Types;


export namespace HOX {

    using HOX::Win32::ComPtr;

    struct BufferAllocation {
        ComPtr<ID3D12Resource> Resource{};
        D3D12MA::Allocation* Allocation{nullptr};
    };

    class MemoryAllocator {
    public:
        MemoryAllocator() = default;
        virtual ~MemoryAllocator() = default;

        MemoryAllocator(const MemoryAllocator&) = delete;
        MemoryAllocator(MemoryAllocator&&) noexcept = delete;
        MemoryAllocator& operator=(const MemoryAllocator&) = delete;
        MemoryAllocator& operator=(MemoryAllocator&&) noexcept = delete;

        void Initialize(ID3D12Device* Device, IDXGIAdapter1* Adapter);
        void ShutDown();

        BufferAllocation Allocate(u64 Size, D3D12_HEAP_TYPE HeapType, D3D12_RESOURCE_STATES InitialState, D3D12_RESOURCE_FLAGS = D3D12_RESOURCE_FLAG_NONE);
        void FreeAllocation(BufferAllocation& Allocation);

    private:
        D3D12MA::Allocator* m_Allocator{nullptr};

        bool m_bReleased{false};
    };
}