//
// Created by capma on 27-Jan-26.
//

module;
#include <d3d12.h>

module HOX.DescriptorHeap;

import std;
import HOX.Types;
import HOX.Win32;
import HOX.Logger;

namespace HOX {
    bool DescriptorHeap::Initialize(ID3D12Device *Device, u32 NumDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE Type,
        bool ShaderVisible) {

        m_NumDescriptors = NumDescriptors;
        m_DescriptorSize = Device->GetDescriptorHandleIncrementSize(Type);

        D3D12_DESCRIPTOR_HEAP_DESC HeapDesc = {};
        HeapDesc.Type = Type;
        HeapDesc.NumDescriptors = m_NumDescriptors;
        HeapDesc.Flags = ShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        HeapDesc.NodeMask = 0;

        HRESULT Hr = Device->CreateDescriptorHeap(&HeapDesc,HOX::Win32::UuidOf<ID3D12DescriptorHeap>(),HOX::Win32::PpvArgs(m_DescriptorHeap.ReleaseAndGetAddressOf()));

        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to create descriptor heap");
            return false;
        }

        Logger::LogMessage(Severity::Info, std::format("Created descriptor heap with {} descriptors",NumDescriptors));

        return true;

    }

    u32 DescriptorHeap::Allocate() {

        if (m_NextFreeIndex >= m_NumDescriptors) {
            Logger::LogMessage(Severity::Error, "Descriptor heap is full");
            return 0;
        }
        return m_NextFreeIndex++;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::GetCPUHandle(u32 Index) const {
        D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle = m_DescriptorHeap->GetCPUDescriptorHandleForHeapStart();
        CPUHandle.ptr += static_cast<SIZE_T>(Index) * m_DescriptorSize;
        return CPUHandle;
    }

    D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::GetGPUHandle(u32 Index) const {
        D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle = m_DescriptorHeap->GetGPUDescriptorHandleForHeapStart();
        GpuHandle.ptr += static_cast<SIZE_T>(Index) * m_DescriptorSize;
        return GpuHandle;
    }
}

