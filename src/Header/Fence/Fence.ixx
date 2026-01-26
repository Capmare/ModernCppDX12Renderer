//
// Created by david on 12/23/2025.
//

module;
#include <windows.h>
#include <wrl/client.h>
#include <d3d12.h>
#include <cstdint>

// Engine exports
export module HOX.Fence;

export namespace HOX {
    using Microsoft::WRL::ComPtr;

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
        uint64_t &GetFenceValue() { return m_FenceValue; }


    private:
        ComPtr<ID3D12Fence> m_Fence{};
        HANDLE m_FenceEvent{};
        uint64_t m_FenceValue{};


    };
} // HOX

