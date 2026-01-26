//
// Created by david on 10/29/2025.
//

module;
#include <d3d12.h>
#include <dxgi1_5.h>


export module HOX.Renderer;

import std;
import HOX.Win32;
import HOX.Types;
import HOX.DeviceManager;
import HOX.SwapChain;
import HOX.Fence;
import HOX.Context;

export namespace HOX {

    using HOX::Win32::ComPtr;

    class Renderer {
    public:
        Renderer();
        ~Renderer() = default;

        // Prevent copy and move
        Renderer(const Renderer &) = delete;
        Renderer &operator=(const Renderer &) = delete;
        Renderer(Renderer &&) = delete;
        Renderer &operator=(Renderer &&) = delete;

        void InitializeRenderer(HWND Hwnd);
        void Render();
        // This should be part of the engine
        void Update();
        void CleanUpRenderer();

        void ResizeSwapChain(const u32 Width, const u32 Height);
    private:
        // Swapchain || Synchronization
        std::unique_ptr<HOX::Fence> m_Fence{};
        void SetFullScreen(HWND Hwnd, bool FullScreen);

        RECT m_WindowRect{};
        bool m_bFullScreen{false};

        // Descriptor heap (descriptor sets in vulkan)
        ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(ComPtr<ID3D12Device10> Device,D3D12_DESCRIPTOR_HEAP_TYPE Type, u32 NumDescriptors);

        // RTV
        void UpdateRenderTarget(ComPtr<ID3D12Device10> Device, ComPtr<IDXGISwapChain4> SwapChain,ComPtr<ID3D12DescriptorHeap> DescriptorHeap);
        ComPtr<ID3D12DescriptorHeap> m_RTVDescriptorHeap{};
        UINT m_RTVDescriptorSize{};

        std::unique_ptr<DeviceManager> m_DeviceManager{};
        ComPtr<ID3D12CommandAllocator> m_CommandAllocators[MaxFrames]{};
        ComPtr<ID3D12GraphicsCommandList7> m_CommandList{};

        std::unique_ptr<Swapchain> m_SwapChain{};

        bool m_bTearingSupported{false};

    };
} // HOX

