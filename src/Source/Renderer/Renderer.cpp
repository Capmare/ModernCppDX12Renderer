//
// Created by david on 10/29/2025.
//


module;
#include <cstdint>
#include <windows.h>
#include <wrl/client.h>
#include <d3d12.h>
#include "../../d3dx12.h"
#include <DXGI.h>
#include <cstdio>


module HOX.Renderer;

import std;
import HOX.Logger;


namespace HOX {
    Renderer::Renderer() {
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

        for (std::uint32_t i = 0; i < MaxFrames; i++) {
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

        // Query adapter and create device
        m_DeviceManager = std::make_unique<DeviceManager>();
        m_DeviceManager->Initialize();

        m_bTearingSupported = m_DeviceManager->CheckTearingSupport();

        GetDeviceContext().m_CommandSystem = std::make_unique<CommandSystem>();
        GetDeviceContext().m_CommandSystem->Initialize();

        m_SwapChain = std::make_unique<Swapchain>();
        m_SwapChain->Initialize();

        m_RTVDescriptorHeap = CreateDescriptorHeap(GetDeviceContext().m_Device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
                                                   MaxFrames);
        m_RTVDescriptorSize = GetDeviceContext().m_Device->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        UpdateRenderTarget(GetDeviceContext().m_Device, m_SwapChain->GetSwapChain(), m_RTVDescriptorHeap);

        for (uint32_t i = 0; i < MaxFrames; i++) {
            m_CommandAllocators[i] = GetDeviceContext().m_CommandSystem->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT);
        }

        m_CommandList = GetDeviceContext().m_CommandSystem->CreateCommandList(GetDeviceContext().m_Device,
                                                           m_CommandAllocators[m_SwapChain->GetCurrentBackBufferIndex()],
                                                           D3D12_COMMAND_LIST_TYPE_DIRECT);


        // Create fence and event handle
        m_Fence = std::make_unique<Fence>();
        if (!m_Fence) {
            Logger::LogMessage(Severity::Error, "Failed to create fence.");
        }



        ResizeSwapChain(GetDeviceContext().m_WindowWidth, GetDeviceContext().m_WindowHeight);

        DeviceManager::PrintDebugMessages(GetDeviceContext().m_Device.Get());
    }


    void Renderer::Render() {
        // Get current frame index from the swap chain
        auto CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();

        // Grab the command allocator and back buffer for this frame
        auto CommandAllocator = m_CommandAllocators[CurrentBackBufferIndex];
        auto BackBuffer = m_SwapChain->GetBackBuffer(CurrentBackBufferIndex);

        // Safety checks
        if (!BackBuffer) {
            Logger::LogMessage(Severity::Error, "BackBuffer is null at frame index " + std::to_string(CurrentBackBufferIndex));
            return;
        }
        if (!CommandAllocator) {
            Logger::LogMessage(Severity::Error,
                               "CommandAllocator is null at frame index " + std::to_string(CurrentBackBufferIndex));
            return;
        }
        if (!m_CommandList) {
            Logger::LogMessage(Severity::Error, "CommandList is null");
            return;
        }
        if (!m_RTVDescriptorHeap) {
            Logger::LogMessage(Severity::Error, "RTV Descriptor Heap is null");
            return;
        }

        GetDeviceContext().m_CommandSystem->WaitForFenceValues(
             m_Fence->GetFence(),
             m_SwapChain->m_FrameFenceValues[CurrentBackBufferIndex],
             m_Fence->GetFenceEvent()
         );

        // Reset allocator
        HRESULT Hr = CommandAllocator->Reset();
        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to reset command allocator.");
            return;
        }

        // Reset command list
        Hr = m_CommandList->Reset(CommandAllocator.Get(), nullptr);
        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to reset command list.");
            return;
        }

        {
            // Transition back buffer to render target
            CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                BackBuffer.Get(),
                D3D12_RESOURCE_STATE_PRESENT,
                D3D12_RESOURCE_STATE_RENDER_TARGET
            );
            m_CommandList->ResourceBarrier(1, &barrier);

            const FLOAT clearColor[4] = {0.4f, 0.4f, 0.4f, 1.0f};

            // Clear render target
            CD3DX12_CPU_DESCRIPTOR_HANDLE RTV(
                m_RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
                CurrentBackBufferIndex,
                m_RTVDescriptorSize
            );

            m_CommandList->ClearRenderTargetView(RTV, clearColor, 0, nullptr);
        }

        {
            // Transition back buffer to present
            CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
                BackBuffer.Get(),
                D3D12_RESOURCE_STATE_RENDER_TARGET,
                D3D12_RESOURCE_STATE_PRESENT
            );
            m_CommandList->ResourceBarrier(1, &barrier);

            // Close command list
            Hr = m_CommandList->Close();
            if (FAILED(Hr)) {
                Logger::LogMessage(Severity::Error, "Failed to close command list.");
                return;
            }

            // Execute command list
            ID3D12CommandList *Lists[] = {m_CommandList.Get()};
            GetDeviceContext().m_CommandQueue->ExecuteCommandLists(_countof(Lists), Lists);

            // Signal fence for this frame
            m_SwapChain->m_FrameFenceValues[CurrentBackBufferIndex] = GetDeviceContext().m_CommandSystem->Signal(
                m_Fence->GetFence(),
                m_Fence->GetFenceValue()
            );



            // Present
            const UINT syncInterval = GetDeviceContext().m_bUseVSync ? 1 : 0;
            const UINT presentFlags = m_bTearingSupported && !GetDeviceContext().m_bUseVSync
                                          ? DXGI_PRESENT_ALLOW_TEARING
                                          : 0;
            Hr = m_SwapChain->GetSwapChain()->Present(syncInterval, presentFlags);
            if (FAILED(Hr)) {
                Logger::LogMessage(Severity::Error, "SwapChain Present failed.");
            }


        }

        DeviceManager::PrintDebugMessages(GetDeviceContext().m_Device.Get());
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
        GetDeviceContext().m_CommandSystem->FlushCommands(m_Fence->GetFence(), m_Fence->GetFenceValue(), m_Fence->GetFenceEvent());
        CloseHandle(m_Fence->GetFenceEvent());
    }

    void Renderer::ResizeSwapChain(const uint32_t Width, const uint32_t Height) {
        if (!m_SwapChain) { Logger::LogMessage(Severity::ErrorNoCrash, "Failed to resize window due to swapchain."); return; }
        if (!m_Fence) { Logger::LogMessage(Severity::ErrorNoCrash, "Failed to resize window due to fence."); return; }

        m_SwapChain->Resize(m_Fence.get(),Width, Height);
        UpdateRenderTarget(GetDeviceContext().m_Device,m_SwapChain->GetSwapChain(),m_RTVDescriptorHeap);
    }
} // HOX
