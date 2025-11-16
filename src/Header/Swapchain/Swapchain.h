//
// Created by capma on 16-Nov-25.
//

#ifndef SWAPCHAIN_H
#define SWAPCHAIN_H

#include "../../pch.h"

namespace HOX {

class Swapchain {

public:
    Swapchain() = default;
    virtual ~Swapchain() = default;

    Swapchain(const Swapchain&) = delete;
    Swapchain(Swapchain&&) noexcept = delete;
    Swapchain& operator=(const Swapchain&) = delete;
    Swapchain& operator=(Swapchain&&) noexcept = delete;

    void Initialize();

    uint8_t GetCurrentBackBufferIndex() const { return m_SwapChain->GetCurrentBackBufferIndex(); }
    ComPtr<IDXGISwapChain4> GetSwapChain() const { return m_SwapChain; }
    ComPtr<ID3D12Resource> GetCurrentBackBuffer() const { return BackBuffers[m_SwapChain->GetCurrentBackBufferIndex()]; }
    void UpdateBackBuffer(const ComPtr<ID3D12Resource2> &NewBackBuffer, uint8_t Location);
private:
    ComPtr<IDXGISwapChain4> CreateSwapChain(uint32_t BufferCount);
    ComPtr<ID3D12Resource> BackBuffers[m_MaxFrames]{};
    ComPtr<IDXGISwapChain4> m_SwapChain{};
};


} // HOX

#endif //SWAPCHAIN_H
