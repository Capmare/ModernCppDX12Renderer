//
// Created by capma on 26-Jan-26.
//

module;
#include <d3d12.h>
#include <dxgi1_6.h>
#include "D3D12MemAlloc.h"


module HOX.MemoryAllocator;

import HOX.Logger;
import HOX.Context;

namespace HOX {


    void MemoryAllocator::Initialize(ID3D12Device *Device, IDXGIAdapter1 *Adapter) {
        D3D12MA::ALLOCATOR_DESC AllocatorDesc = {};
        AllocatorDesc.pDevice = Device;
        AllocatorDesc.pAdapter = Adapter;
        AllocatorDesc.Flags = D3D12MA::ALLOCATOR_FLAG_NONE;

        HRESULT hr = D3D12MA::CreateAllocator(&AllocatorDesc, &m_Allocator);
        if (FAILED(hr)) {
            Logger::LogMessage(Severity::Error, "Failed to create allocator");
        }
        else Logger::LogMessage(Severity::Info, "Created allocator");

        GetDeviceContext().m_Cleaner->AddToCleaner([this]() {
            this->ShutDown();
        });
    }

    void MemoryAllocator::ShutDown() {
        if (m_Allocator) {
            m_Allocator->Release();
            m_Allocator = nullptr;
        }
    }

    BufferAllocation MemoryAllocator::Allocate(u64 Size, D3D12_HEAP_TYPE HeapType, D3D12_RESOURCE_STATES InitialState, D3D12_RESOURCE_FLAGS Flags) {
        BufferAllocation Allocation = {};

        D3D12MA::ALLOCATION_DESC AllocationDesc{};
        AllocationDesc.HeapType = HeapType;

        D3D12_RESOURCE_DESC ResourceDesc{};
        ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        ResourceDesc.Alignment = 0;
        ResourceDesc.Width = Size;
        ResourceDesc.Height = 1;
        ResourceDesc.DepthOrArraySize = 1;
        ResourceDesc.MipLevels = 1;
        ResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
        ResourceDesc.SampleDesc.Count = 1;
        ResourceDesc.SampleDesc.Quality = 0;
        ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ResourceDesc.Flags = Flags;

        ComPtr<ID3D12Resource2> Resource = nullptr;
        HRESULT Hr = m_Allocator->CreateResource(
            &AllocationDesc,
            &ResourceDesc,
            InitialState,
            nullptr,
            &Allocation.Allocation,
            HOX::Win32::UuidOf<ID3D12Resource2>(),
            HOX::Win32::PpvArgs(Resource.ReleaseAndGetAddressOf())
        );

        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to create resource");
        }
        else {
            Allocation.Resource.Attach(Resource.Get());
        }

        return Allocation;

    }

    void MemoryAllocator::FreeAllocation(BufferAllocation &Allocation) {

        if (Allocation.Allocation) {
            Allocation.Allocation->Release();
            Allocation.Allocation = nullptr;
        }
        Allocation.Resource.Reset();

    }
}
