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
import HOX.Context;

namespace HOX {
    bool DescriptorHeap::Initialize(D3D12_DESCRIPTOR_HEAP_TYPE Type, bool ShaderVisible, u32 InitialCapacity) {
        m_Device = GetDeviceContext().m_Device.Get();
        m_HeapType = Type;
        m_bShaderVisible = ShaderVisible;
        m_Capacity = InitialCapacity;
        m_DescriptorSize = m_Device->GetDescriptorHandleIncrementSize(Type);

        D3D12_DESCRIPTOR_HEAP_DESC HeapDesc = {};
        HeapDesc.Type = Type;
        HeapDesc.NumDescriptors = m_Capacity;
        HeapDesc.Flags = ShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        HeapDesc.NodeMask = 0;

        HRESULT Hr = m_Device->CreateDescriptorHeap(&HeapDesc, HOX::Win32::UuidOf<ID3D12DescriptorHeap>(), HOX::Win32::PpvArgs(m_DescriptorHeap.ReleaseAndGetAddressOf()));

        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to create descriptor heap");
            return false;
        }

        Logger::LogMessage(Severity::Info, std::format("Created descriptor heap with capacity {}", m_Capacity));

        return true;
    }

    bool DescriptorHeap::Grow() {
        u32 NewCapacity = m_Capacity * GrowthFactor;

        D3D12_DESCRIPTOR_HEAP_DESC HeapDesc = {};
        HeapDesc.Type = m_HeapType;
        HeapDesc.NumDescriptors = NewCapacity;
        HeapDesc.Flags = m_bShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        HeapDesc.NodeMask = 0;

        ComPtr<ID3D12DescriptorHeap> NewHeap{};
        HRESULT Hr = m_Device->CreateDescriptorHeap(&HeapDesc, HOX::Win32::UuidOf<ID3D12DescriptorHeap>(), HOX::Win32::PpvArgs(NewHeap.ReleaseAndGetAddressOf()));

        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to grow descriptor heap");
            return false;
        }

        // Copy existing descriptors to the new heap
        if (m_NextFreeIndex > 0) {
            m_Device->CopyDescriptorsSimple(
                m_NextFreeIndex,
                NewHeap->GetCPUDescriptorHandleForHeapStart(),
                m_DescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
                m_HeapType);
        }

        m_DescriptorHeap = std::move(NewHeap);
        m_Capacity = NewCapacity;

        Logger::LogMessage(Severity::Info, std::format("Grew descriptor heap to capacity {}", m_Capacity));

        return true;
    }

    u32 DescriptorHeap::Allocate() {
        if (m_NextFreeIndex >= m_Capacity) {
            if (!Grow()) {
                Logger::LogMessage(Severity::Error, "Failed to allocate descriptor - heap full and cannot grow");
                return 0;
            }
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

