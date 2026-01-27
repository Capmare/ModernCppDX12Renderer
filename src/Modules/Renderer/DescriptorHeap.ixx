//
// Created by capma on 27-Jan-26.
//

module;
#include <d3d12.h>

export module HOX.DescriptorHeap;

import std;
import HOX.Types;
import HOX.Win32;

export namespace HOX {
    using HOX::Win32::ComPtr;

    class DescriptorHeap {
    public:
        static constexpr u32 DefaultInitialCapacity = 64;
        static constexpr u32 GrowthFactor = 2;

        DescriptorHeap() = default;
        virtual ~DescriptorHeap() = default;

        DescriptorHeap(const DescriptorHeap&) = delete;
        DescriptorHeap& operator=(const DescriptorHeap&) = delete;
        DescriptorHeap(DescriptorHeap&&) noexcept = default;
        DescriptorHeap& operator=(DescriptorHeap&&) noexcept = default;

        bool Initialize(D3D12_DESCRIPTOR_HEAP_TYPE Type, bool ShaderVisible, u32 InitialCapacity = DefaultInitialCapacity);

        u32 Allocate();

        D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle(u32 Index) const;
        D3D12_GPU_DESCRIPTOR_HANDLE GetGPUHandle(u32 Index) const;

        ID3D12DescriptorHeap* GetD3D12DescriptorHeap() const { return m_DescriptorHeap.Get(); };

        [[nodiscard]] u32 GetDescriptorSize() const { return m_DescriptorSize; };
        [[nodiscard]] u32 GetCapacity() const { return m_Capacity; };
        [[nodiscard]] u32 GetAllocatedCount() const { return m_NextFreeIndex; };

    private:
        bool Grow();

        ComPtr<ID3D12DescriptorHeap> m_DescriptorHeap{};
        ID3D12Device* m_Device{nullptr};
        D3D12_DESCRIPTOR_HEAP_TYPE m_HeapType{};
        bool m_bShaderVisible{false};

        u32 m_DescriptorSize{};
        u32 m_Capacity{};
        u32 m_NextFreeIndex{};
    };
}
