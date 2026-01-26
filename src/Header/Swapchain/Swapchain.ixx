//
// Created by capma on 16-Nov-25.
//


module;
#include <windows.h>
#include <dxgi1_6.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>

export module HOX.SwapChain;

import std;
import HOX.Fence;
import HOX.Context;


export namespace HOX {
    using Microsoft::WRL::ComPtr;

    class Swapchain {
    public:

        Swapchain() = default;
        virtual ~Swapchain() = default;
        Swapchain(const Swapchain &) = delete;
        Swapchain(Swapchain &&) noexcept = delete;
        Swapchain &operator=(const Swapchain &) = delete;
        Swapchain &operator=(Swapchain &&) noexcept = delete;

        void Initialize();

        void Resize(HOX::Fence *CurrentFence, uint32_t Width, uint32_t Height);

        [[nodiscard]] uint8_t GetCurrentBackBufferIndex() const { return m_SwapChain->GetCurrentBackBufferIndex(); }
        [[nodiscard]] ComPtr<IDXGISwapChain4> GetSwapChain() const { return m_SwapChain; }

        [[nodiscard]] ComPtr<ID3D12Resource> GetCurrentBackBuffer() const {
            return m_BackBuffers.at(m_SwapChain->GetCurrentBackBufferIndex());
        }

        ComPtr<ID3D12Resource> GetBackBuffer(uint32_t Idx) const { return m_BackBuffers.at(Idx); }

        void UpdateBackBuffer(const ComPtr<ID3D12Resource2> &NewBackBuffer, uint8_t Location);

        uint64_t m_FrameFenceValues[MaxFrames]{};

    private:
        ComPtr<IDXGISwapChain4> CreateSwapChain(uint32_t BufferCount);

        std::array<ComPtr<ID3D12Resource>, MaxFrames> m_BackBuffers{};
        ComPtr<IDXGISwapChain4> m_SwapChain{};
    };
} // HOX
