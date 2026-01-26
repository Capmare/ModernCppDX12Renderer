//
// Created by david on 12/23/2025.
//

module;

#include <d3d12.h>

import HOX.Win32;
import HOX.Types;

// Engine exports
export module HOX.Fence;

export namespace HOX {
    using HOX::Win32::ComPtr;

    class Fence {
    public:
        Fence();
        ~Fence();


        // Prevent copy and move2
        Fence(const Fence&) = delete;
        Fence& operator=(const Fence&) = delete;
        Fence(Fence&&) = delete;
        Fence& operator=(Fence&&) = delete;


        ComPtr<ID3D12Fence> CreateFence(ComPtr<ID3D12Device2> Device);

        HANDLE CreateFenceEvent();

        [[nodiscard]] ComPtr<ID3D12Fence> GetFence() const { return m_Fence; }
        [[nodiscard]] HANDLE GetFenceEvent() const { return m_FenceEvent; }
        u64 &GetFenceValue() { return m_FenceValue; }


    private:
        ComPtr<ID3D12Fence> m_Fence{};
        HANDLE m_FenceEvent{};
        u64 m_FenceValue{};


    };
} // HOX

