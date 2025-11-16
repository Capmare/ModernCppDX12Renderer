//
// Created by david on 10/29/2025.
//

#include "../../Header/Renderer/Renderer.h"
#include <chrono>
#include "../../pch.h"

#include "../../Helpers.h"

namespace HOX {
    Renderer::Renderer() {
    }

    ComPtr<ID3D12CommandQueue>
    Renderer::CreateCommandQueue(ComPtr<ID3D12Device10> Device, D3D12_COMMAND_LIST_TYPE Type) {
        ComPtr<ID3D12CommandQueue> CommandQueue{};

        D3D12_COMMAND_QUEUE_DESC QueueDesc = {};
        QueueDesc.Type = Type;
        QueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
        QueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        QueueDesc.NodeMask = 0;

        HRESULT Hr = Device->CreateCommandQueue(&QueueDesc, IID_PPV_ARGS(&CommandQueue));
        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to create command queue.");
        } else {
            Logger::LogMessage(Severity::Info, "Created command queue.");
        }

        return CommandQueue;
    }

    ComPtr<ID3D12CommandAllocator> Renderer::CreateCommandAllocator(ComPtr<ID3D12Device10> Device,
                                                                    D3D12_COMMAND_LIST_TYPE Type) {
        ComPtr<ID3D12CommandAllocator> CommandAllocator{};

        HRESULT Hr = Device->CreateCommandAllocator(Type, IID_PPV_ARGS(&CommandAllocator));
        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to create command allocator.");
        } else {
            Logger::LogMessage(Severity::Info, "Created command allocator.");
        }

        return CommandAllocator;
    }

    ComPtr<ID3D12GraphicsCommandList7> Renderer::CreateCommandList(ComPtr<ID3D12Device10> Device,
                                                                   ComPtr<ID3D12CommandAllocator> CommandAllocator,
                                                                   D3D12_COMMAND_LIST_TYPE Type) {
        ComPtr<ID3D12GraphicsCommandList7> CommandList{};

        HRESULT Hr = Device->CreateCommandList(0, Type, CommandAllocator.Get(), nullptr, IID_PPV_ARGS(&CommandList));
        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to create command list.");
        } else {
            Logger::LogMessage(Severity::Info, "Created command list.");
        }

        Hr = CommandList->Close();
        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to close command list.");
        }

        return CommandList;
    }

    void Renderer::FlushCommands(ComPtr<ID3D12CommandQueue> CommandQueue, ComPtr<ID3D12Fence> Fence,
                                 uint64_t &FenceValue, HANDLE FenceEvent) {
        uint64_t FenceValueForSignal{Signal(CommandQueue, Fence, FenceValue)};
        WaitForFenceValues(Fence, FenceValueForSignal, FenceEvent);
    }

    ComPtr<IDXGISwapChain4> Renderer::CreateSwapChain(HWND Hwnd, ComPtr<ID3D12CommandQueue> CommandQueue,
                                                      uint32_t Width, uint32_t Height, uint32_t BufferCount) {
        ComPtr<IDXGISwapChain4> SwapChain{};
        ComPtr<IDXGIFactory4> Factory{};

        UINT CreateFactoryFlags = 0;

#ifdef _DEBUG
        CreateFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

        HRESULT Hr = CreateDXGIFactory2(CreateFactoryFlags, IID_PPV_ARGS(&Factory));
        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to create DXGIFactory2.");
        } else {
            Logger::LogMessage(Severity::Info, "Created DXGIFactory2.");
        }


        DXGI_SWAP_CHAIN_DESC1 SwapChainDesc{};
        SwapChainDesc.Width = Width;
        SwapChainDesc.Height = Height;
        SwapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        SwapChainDesc.Stereo = FALSE;
        SwapChainDesc.SampleDesc = {1, 0};
        SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        SwapChainDesc.BufferCount = BufferCount;
        SwapChainDesc.Scaling = DXGI_SCALING_STRETCH;
        SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        SwapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
        SwapChainDesc.Flags = /*CheckTearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING :*/ 0;

        ComPtr<IDXGISwapChain1> SwapChain1{};
        Hr = Factory->CreateSwapChainForHwnd(
            CommandQueue.Get(),
            Hwnd,
            &SwapChainDesc,
            nullptr,
            nullptr,
            &SwapChain1);
        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to create swap chain.");
        }

        // Disable alt enter
        Hr = Factory->MakeWindowAssociation(Hwnd,DXGI_MWA_NO_ALT_ENTER);
        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to make window association.");
        } else {
            Logger::LogMessage(Severity::Info, "Created swap chain.");
        }

        Hr = SwapChain1.As(&SwapChain);
        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to get swap chain.");
        } else {
            Logger::LogMessage(Severity::Info, "Created swap chain.");
        }

        return SwapChain;
    }

    ComPtr<ID3D12Fence> Renderer::CreateFence(ComPtr<ID3D12Device2> Device) {
        ComPtr<ID3D12Fence> Fence{};
        HRESULT Hr = Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Fence));
        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to create fence.");
        } else {
            Logger::LogMessage(Severity::Info, "Created fence.");
        }

        return Fence;
    }

    HANDLE Renderer::CreateFenceEvent() {
        HANDLE FenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);\
        if (FenceEvent == nullptr) {
            Logger::LogMessage(Severity::Error, "Failed to create fence event.");
        } else {
            Logger::LogMessage(Severity::Info, "Created fence event.");
        }

        return FenceEvent;
    }

    uint64_t Renderer::Signal(ComPtr<ID3D12CommandQueue> CommandQueue, ComPtr<ID3D12Fence> Fence, uint64_t FenceValue) {
        uint64_t SignalValue = ++FenceValue;
        HRESULT Hr = CommandQueue->Signal(Fence.Get(), SignalValue);

        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to signal fence.");
        } else {
            Logger::LogMessage(Severity::Info, "Signaling fence.");
        }

        return SignalValue;
    }

    void Renderer::WaitForFenceValues(ComPtr<ID3D12Fence> Fence, uint64_t FenceValue, HANDLE FenceEvent) {
        if (Fence->GetCompletedValue() < FenceValue) {
            HRESULT Hr = Fence->SetEventOnCompletion(FenceValue, FenceEvent);
            if (FAILED(Hr)) {
                Logger::LogMessage(Severity::Error, "Failed to set fence event.");
            } else {
                Logger::LogMessage(Severity::Info, "Setting fence event.");
            }

            WaitForSingleObject(FenceEvent, INFINITE);
        }
    }

    void Renderer::SetFullScreen(HWND Hwnd, bool FullScreen) {
        if (m_bFullScreen != FullScreen) {
            m_bFullScreen = FullScreen;
            if (m_bFullScreen) {
                GetWindowRect(Hwnd, &m_WindowRect);

                UINT WindowStyle =
                        WS_OVERLAPPEDWINDOW & ~(
                            WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);

                SetWindowLongW(Hwnd, GWL_STYLE, WindowStyle);

                HMONITOR HMonitor = MonitorFromWindow(Hwnd, MONITOR_DEFAULTTONEAREST);
                MONITORINFOEX MonitorInfo = {};
                MonitorInfo.cbSize = sizeof(MONITORINFOEX);
                GetMonitorInfo(HMonitor, &MonitorInfo);

                SetWindowPos(Hwnd, HWND_TOP,
                             MonitorInfo.rcMonitor.left,
                             MonitorInfo.rcMonitor.top,
                             MonitorInfo.rcMonitor.right - MonitorInfo.rcMonitor.left,
                             MonitorInfo.rcMonitor.bottom - MonitorInfo.rcMonitor.top,
                             SWP_FRAMECHANGED | SWP_NOACTIVATE);
            } else {
                SetWindowLong(Hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW);

                SetWindowPos(Hwnd, HWND_NOTOPMOST,
                             m_WindowRect.left,
                             m_WindowRect.top,
                             m_WindowRect.right - m_WindowRect.left,
                             m_WindowRect.bottom - m_WindowRect.top,
                             SWP_FRAMECHANGED | SWP_NOACTIVATE);

                ShowWindow(Hwnd, SW_NORMAL);
            }
        }
    }

    ComPtr<ID3D12DescriptorHeap> Renderer::CreateDescriptorHeap(ComPtr<ID3D12Device10> Device,
                                                                D3D12_DESCRIPTOR_HEAP_TYPE Type,
                                                                uint32_t NumDescriptors) {
        ComPtr<ID3D12DescriptorHeap> DescriptorHeap{};

        D3D12_DESCRIPTOR_HEAP_DESC HeapDesc{};
        HeapDesc.Type = Type;
        HeapDesc.NumDescriptors = NumDescriptors;

        HRESULT Hr = Device->CreateDescriptorHeap(&HeapDesc, IID_PPV_ARGS(&DescriptorHeap));
        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to create descriptor heap.");
        } else {
            Logger::LogMessage(Severity::Info, "Created descriptor heap.");
        }

        return DescriptorHeap;
    }

    void Renderer::UpdateRenderTarget(ComPtr<ID3D12Device10> Device, ComPtr<IDXGISwapChain4> SwapChain,
                                      ComPtr<ID3D12DescriptorHeap> DescriptorHeap) {
        auto RTVDescriptorSize = Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        D3D12_CPU_DESCRIPTOR_HANDLE RtvHandle(DescriptorHeap->GetCPUDescriptorHandleForHeapStart());

        for (uint32_t i = 0; i < m_MaxFrames; i++) {
            ComPtr<ID3D12Resource2> BackBuffer{};
            HRESULT Hr = SwapChain->GetBuffer(i, IID_PPV_ARGS(&BackBuffer));
            if (FAILED(Hr)) {
                Logger::LogMessage(Severity::Error, "Failed to get back buffer.");
            } else {
                Logger::LogMessage(Severity::Info, "Getting back buffer.");
            }

            Device->CreateRenderTargetView(BackBuffer.Get(), nullptr, RtvHandle);
            BackBuffers[i] = BackBuffer;

            RtvHandle.ptr += RTVDescriptorSize;
        }
    }


    void Renderer::InitializeRenderer(HWND Hwnd) {
        m_Context = std::make_unique<Context>();

        m_DeviceManager = std::make_unique<DeviceManager>();
        m_DeviceManager->Initialize(m_Context);


        m_bTearingSupported = m_DeviceManager->CheckTearingSupport();

        m_CommandQueue = CreateCommandQueue(m_Context->m_Device, D3D12_COMMAND_LIST_TYPE_DIRECT);

        m_SwapChain = CreateSwapChain(Hwnd, m_CommandQueue, m_WindowWidth, m_WindowHeight, m_MaxFrames);

        m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();

        m_RTVDescriptorHeap = CreateDescriptorHeap(m_Context->m_Device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
                                                   m_MaxFrames);
        m_RTVDescriptorSize = m_Context->m_Device->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        UpdateRenderTarget(m_Context->m_Device, m_SwapChain, m_RTVDescriptorHeap);

        for (uint32_t i = 0; i < m_MaxFrames; i++) {
            m_CommandAllocators[i] = CreateCommandAllocator(m_Context->m_Device, D3D12_COMMAND_LIST_TYPE_DIRECT);
        }

        m_CommandList = CreateCommandList(m_Context->m_Device, m_CommandAllocators[m_CurrentBackBufferIndex],
                                          D3D12_COMMAND_LIST_TYPE_DIRECT);

        m_Fence = CreateFence(m_Context->m_Device);
        m_FenceEvent = CreateFenceEvent();
    }

    void Renderer::Render() {
        auto CommandAllocator = m_CommandAllocators[m_CurrentBackBufferIndex];
        auto BackBuffer = BackBuffers[m_CurrentBackBufferIndex];

        HRESULT Hr = CommandAllocator->Reset();
        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to reset command allocator.");
        }

        Hr = m_CommandList->Reset(CommandAllocator.Get(), nullptr);
        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to reset command list.");
        }

        // Clear render target
        {
            D3D12_RESOURCE_BARRIER ResourceBarrier{
                CD3DX12_RESOURCE_BARRIER::Transition(
                    BackBuffer.Get(),
                    D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET)
            };
            m_CommandList->ResourceBarrier(1, &ResourceBarrier);

            FLOAT ClearColor[4] = {
                .4f, .4f, .4f, 1.f
            };
            CD3DX12_CPU_DESCRIPTOR_HANDLE RTV(
                m_RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
                m_CurrentBackBufferIndex, m_RTVDescriptorSize

            );
            m_CommandList->ClearRenderTargetView(RTV, ClearColor, 0, nullptr);
        }

        // Present to screen
        {
            CD3DX12_RESOURCE_BARRIER ResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
                BackBuffer.Get(),
                D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
            m_CommandList->ResourceBarrier(1, &ResourceBarrier);

            ThrowIfFailed(m_CommandList->Close());

            ID3D12CommandList *const CommandLists[] = {
                m_CommandList.Get()
            };
            m_CommandQueue->ExecuteCommandLists(_countof(CommandLists), CommandLists);
            m_FrameFenceValues[m_CurrentBackBufferIndex] = Signal(m_CommandQueue, m_Fence, m_FenceValue);

            UINT SyncInterval = m_Context->m_bUseVSync ? 1 : 0;
            UINT PresentFlags = m_bTearingSupported && !m_Context->m_bUseVSync
                                    ? DXGI_PRESENT_ALLOW_TEARING
                                    : 0;

            m_SwapChain->Present(SyncInterval, PresentFlags);

            m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();

            WaitForFenceValues(m_Fence, m_FrameFenceValues[m_CurrentBackBufferIndex], m_FenceEvent);
        }
    }

    void Renderer::Update() {
        static uint64_t frameCounter = 0;
        static double elapsedSeconds = 0.0;
        static std::chrono::high_resolution_clock clock;
        static auto t0 = clock.now();

        frameCounter++;
        auto t1 = clock.now();
        auto deltaTime = t1 - t0;
        t0 = t1;

        elapsedSeconds += deltaTime.count() * 1e-9;
        if (elapsedSeconds > 1.0) {
            char buffer[500];
            auto fps = frameCounter / elapsedSeconds;
            sprintf_s(buffer, 500, "FPS: %f\n", fps);
            OutputDebugString(buffer);

            frameCounter = 0;
            elapsedSeconds = 0.0;
        }
    }

    void Renderer::CleanUpRenderer() {
        FlushCommands(m_CommandQueue, m_Fence, m_FenceValue, m_FenceEvent);
        CloseHandle(m_FenceEvent);
    }
} // HOX
