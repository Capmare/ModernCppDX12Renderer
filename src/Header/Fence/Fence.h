//
// Created by david on 12/23/2025.
//

#ifndef MODERNCPPDX12RENDERER_FENCE_H
#define MODERNCPPDX12RENDERER_FENCE_H

#include "../../pch.h"

namespace HOX {
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

        ComPtr<ID3D12Fence> GetFence() const { return m_Fence; }
        HANDLE GetFenceEvent() const { return m_FenceEvent; }
        uint64_t &GetFenceValue() { return m_FenceValue; }


    private:
        ComPtr<ID3D12Fence> m_Fence{};
        HANDLE m_FenceEvent{};
        uint64_t m_FenceValue{};


    };
} // HOX

#endif //MODERNCPPDX12RENDERER_FENCE_H