//
// Created by david on 10/29/2025.
//

#ifndef MODERNCPPDX12RENDERER_RENDERER_H
#define MODERNCPPDX12RENDERER_RENDERER_H


#include <memory>

#include "../Commands/CommandSystem.h"
#include "../Device/DeviceManager.h"
#include "../Fence/Fence.h"
#include "../ResourceManagement/Context.h"
#include "../Swapchain/Swapchain.h"

namespace HOX {

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

        void ResizeSwapChain(const uint32_t Width, const uint32_t Height);
    private:
        // Swapchain || Synchronization
        std::unique_ptr<HOX::Fence> m_Fence{};
        void SetFullScreen(HWND Hwnd, bool FullScreen);

        RECT m_WindowRect{};
        bool m_bFullScreen{false};

        // Descriptor heap (descriptor sets in vulkan)
        ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(ComPtr<ID3D12Device10> Device,D3D12_DESCRIPTOR_HEAP_TYPE Type, uint32_t NumDescriptors);

        // RTV
        void UpdateRenderTarget(ComPtr<ID3D12Device10> Device, ComPtr<IDXGISwapChain4> SwapChain,ComPtr<ID3D12DescriptorHeap> DescriptorHeap);
        ComPtr<ID3D12DescriptorHeap> m_RTVDescriptorHeap{};
        UINT m_RTVDescriptorSize{};

        std::unique_ptr<DeviceManager> m_DeviceManager{};
        ComPtr<ID3D12CommandAllocator> m_CommandAllocators[m_MaxFrames]{};
        ComPtr<ID3D12GraphicsCommandList7> m_CommandList{};

        std::unique_ptr<Swapchain> m_SwapChain{};

        bool m_bTearingSupported{false};

    };
} // HOX

#endif //MODERNCPPDX12RENDERER_RENDERER_H
