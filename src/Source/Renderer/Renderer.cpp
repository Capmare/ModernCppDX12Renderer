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
            m_SwapChain->UpdateBackBuffer(BackBuffer,i);

            RtvHandle.ptr += RTVDescriptorSize;
        }
    }


    void Renderer::InitializeRenderer(HWND Hwnd) {
        GetDeviceContext().Hwnd = Hwnd;

        m_DeviceManager = std::make_unique<DeviceManager>();
        m_DeviceManager->Initialize();


        m_bTearingSupported = m_DeviceManager->CheckTearingSupport();

        m_CommandSystem = std::make_unique<CommandSystem>();
        m_CommandSystem->Initialize();

        m_SwapChain = std::make_unique<Swapchain>();
        m_SwapChain->Initialize();

        m_RTVDescriptorHeap = CreateDescriptorHeap(GetDeviceContext().m_Device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
                                                   m_MaxFrames);
        m_RTVDescriptorSize = GetDeviceContext().m_Device->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        UpdateRenderTarget(GetDeviceContext().m_Device, m_SwapChain->GetSwapChain(), m_RTVDescriptorHeap);

        for (uint32_t i = 0; i < m_MaxFrames; i++) {
            m_CommandAllocators[i] = m_CommandSystem->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT);
        }

        m_CommandList = m_CommandSystem->CreateCommandList(GetDeviceContext().m_Device,
                                                           m_CommandAllocators[m_SwapChain->GetCurrentBackBufferIndex()],
                                                           D3D12_COMMAND_LIST_TYPE_DIRECT);

        m_Fence = CreateFence(GetDeviceContext().m_Device);
        m_FenceEvent = CreateFenceEvent();
    }

    void Renderer::Render() {
        auto CommandAllocator = m_CommandAllocators[m_SwapChain->GetCurrentBackBufferIndex()];
        auto BackBuffer = m_SwapChain->GetCurrentBackBuffer();

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
                m_SwapChain->GetCurrentBackBufferIndex(), m_RTVDescriptorSize

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
            GetDeviceContext().m_CommandQueue->ExecuteCommandLists(_countof(CommandLists), CommandLists);
            m_FrameFenceValues[m_SwapChain->GetCurrentBackBufferIndex()] = m_CommandSystem->Signal(m_Fence, m_FenceValue);

            UINT SyncInterval = GetDeviceContext().m_bUseVSync ? 1 : 0;
            UINT PresentFlags = m_bTearingSupported && !GetDeviceContext().m_bUseVSync
                                    ? DXGI_PRESENT_ALLOW_TEARING
                                    : 0;

            m_SwapChain->GetSwapChain()->Present(SyncInterval, PresentFlags);

            m_CommandSystem->WaitForFenceValues(m_Fence, m_FrameFenceValues[m_SwapChain->GetCurrentBackBufferIndex()], m_FenceEvent);
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
        m_CommandSystem->FlushCommands(m_Fence, m_FenceValue, m_FenceEvent);
        CloseHandle(m_FenceEvent);
    }
} // HOX
