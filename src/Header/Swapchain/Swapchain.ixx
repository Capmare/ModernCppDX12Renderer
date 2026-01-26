//
// Created by capma on 16-Nov-25.
//


module;
#include <dxgi1_6.h>
#include <d3d12.h>


export module HOX.SwapChain;

import HOX.Win32;
import std;
import HOX.Types;
import HOX.Fence;
import HOX.Context;


export namespace HOX {

    class Swapchain {
    public:

        Swapchain() = default;
        virtual ~Swapchain() = default;
        Swapchain(const Swapchain &) = delete;
        Swapchain(Swapchain &&) noexcept = delete;
        Swapchain &operator=(const Swapchain &) = delete;
        Swapchain &operator=(Swapchain &&) noexcept = delete;

        void Initialize();

        void Resize(HOX::Fence *CurrentFence, u32 Width, u32 Height);

        [[nodiscard]] u8 GetCurrentBackBufferIndex() const { return m_SwapChain->GetCurrentBackBufferIndex(); }
        [[nodiscard]] ComPtr<IDXGISwapChain4> GetSwapChain() const { return m_SwapChain; }

        [[nodiscard]] ComPtr<ID3D12Resource> GetCurrentBackBuffer() const {
            return m_BackBuffers.at(m_SwapChain->GetCurrentBackBufferIndex());
        }

        ComPtr<ID3D12Resource> GetBackBuffer(u32 Idx) const { return m_BackBuffers.at(Idx); }

        void UpdateBackBuffer(const ComPtr<ID3D12Resource2> &NewBackBuffer, u8 Location);

        u64 m_FrameFenceValues[MaxFrames]{};

    private:
        ComPtr<IDXGISwapChain4> CreateSwapChain(u32 BufferCount);

        std::array<ComPtr<ID3D12Resource>, MaxFrames> m_BackBuffers{};
        ComPtr<IDXGISwapChain4> m_SwapChain{};
    };
} // HOX
