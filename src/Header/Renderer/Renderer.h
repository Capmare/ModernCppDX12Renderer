//
// Created by david on 10/29/2025.
//

#ifndef MODERNCPPDX12RENDERER_RENDERER_H
#define MODERNCPPDX12RENDERER_RENDERER_H


#include <memory>
#include "../ResourceManagement/DeviceContext.h"

namespace HOX {
    static constexpr uint8_t m_MaxFrames{3};

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

        std::unique_ptr<DeviceContext> m_DeviceContext{};

        bool m_bUseWarp{false};

        uint32_t m_WindowWidth{0};
        uint32_t m_WindowHeight{0};

    private:
        void EnableDebugLayer();

        // Devices
        ComPtr<IDXGIAdapter4> QueryDx12Adapter();
        ComPtr<ID3D12Device10> CreateDevice();
        bool CheckTearingSupport();

        // Commands
        ComPtr<ID3D12CommandQueue> CreateCommandQueue(ComPtr<ID3D12Device10> Device, D3D12_COMMAND_LIST_TYPE Type);
        ComPtr<ID3D12CommandAllocator> CreateCommandAllocator(ComPtr<ID3D12Device10> Device,D3D12_COMMAND_LIST_TYPE Type);

        ComPtr<ID3D12GraphicsCommandList7> CreateCommandList(ComPtr<ID3D12Device10> Device,
                                                             ComPtr<ID3D12CommandAllocator> CommandAllocator,
                                                             D3D12_COMMAND_LIST_TYPE Type);
        void FlushCommands(ComPtr<ID3D12CommandQueue> CommandQueue, ComPtr<ID3D12Fence> Fence, uint64_t &FenceValue,HANDLE FenceEvent);

        ComPtr<ID3D12GraphicsCommandList7> m_CommandList{};
        ComPtr<ID3D12CommandAllocator> m_CommandAllocators[m_MaxFrames]{};
        ComPtr<ID3D12CommandQueue> m_CommandQueue{};

        // Swapchain
        ComPtr<IDXGISwapChain4> CreateSwapChain(HWND Hwnd, ComPtr<ID3D12CommandQueue> CommandQueue, uint32_t Width,
                                                uint32_t Height, uint32_t BufferCount);
        ComPtr<ID3D12Resource> BackBuffers[m_MaxFrames]{};
        UINT m_CurrentBackBufferIndex{};
        ComPtr<IDXGISwapChain4> m_SwapChain{};

        // Swapchain || Synchronization
        ComPtr<ID3D12Fence> CreateFence(ComPtr<ID3D12Device2> Device);
        HANDLE CreateFenceEvent();
        uint64_t Signal(ComPtr<ID3D12CommandQueue> CommandQueue, ComPtr<ID3D12Fence> Fence, uint64_t FenceValue);
        void WaitForFenceValues(ComPtr<ID3D12Fence> Fence, uint64_t FenceValue, HANDLE FenceEvent);
        void SetFullScreen(HWND Hwnd, bool FullScreen);

        RECT m_WindowRect{};
        uint64_t m_FrameFenceValues[m_MaxFrames]{};
        ComPtr<ID3D12Fence> m_Fence{};
        HANDLE m_FenceEvent{};
        uint64_t m_FenceValue{};
        bool m_bFullScreen{false};

        // Descriptor heap (descriptor sets in vulkan)
        ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(ComPtr<ID3D12Device10> Device,D3D12_DESCRIPTOR_HEAP_TYPE Type, uint32_t NumDescriptors);

        // RTV
        void UpdateRenderTarget(ComPtr<ID3D12Device10> Device, ComPtr<IDXGISwapChain4> SwapChain,ComPtr<ID3D12DescriptorHeap> DescriptorHeap);
        ComPtr<ID3D12DescriptorHeap> m_RTVDescriptorHeap{};
        UINT m_RTVDescriptorSize{};

    };
} // HOX

#endif //MODERNCPPDX12RENDERER_RENDERER_H
