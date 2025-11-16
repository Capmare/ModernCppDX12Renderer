//
// Created by david on 10/29/2025.
//

#include "../../Header/Renderer/Renderer.h"
#include <chrono>
#include "../../pch.h"

#include "../../Helpers.h"

namespace HOX {
    Renderer::Renderer() {
        m_DeviceContext = std::make_unique<DeviceContext>();
    }

    void Renderer::EnableDebugLayer() {
#ifdef _DEBUG
        ComPtr<ID3D12Debug> DebugInterface{};
        if (FAILED(D3D12GetDebugInterface(IID_PPV_ARGS(&DebugInterface)))) {
            HOX::Logger::LogMessage(Severity::Error, "Failed to get D3D12 debug layer.");
        } else {
            Logger::LogMessage(Severity::Info, "D3D12 debug layer enabled.");
        }

        DebugInterface->EnableDebugLayer();
#endif
    }

    ComPtr<IDXGIAdapter4> Renderer::QueryDx12Adapter() {
        ComPtr<IDXGIFactory7> DXGIFactory{};
        UINT CreateFactoryFlags = 0;
#ifdef _DEBUG
        CreateFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

        if (FAILED(CreateDXGIFactory2(CreateFactoryFlags, IID_PPV_ARGS(&DXGIFactory)))) {
            Logger::LogMessage(Severity::Error, "Failed to create DXGIFactory.");
            return nullptr;
        } else {
            Logger::LogMessage(Severity::Info, "DXGIFactory created.");
        }

        ComPtr<IDXGIAdapter1> DXGIAdapter1{};
        ComPtr<IDXGIAdapter4> DXGIAdapter4{};

        if (m_bUseWarp) {
            HRESULT hr = DXGIFactory->EnumWarpAdapter(IID_PPV_ARGS(&DXGIAdapter1));
            if (FAILED(hr)) {
                Logger::LogMessage(Severity::Error, "Failed to enumerate warp adapter.");
                return nullptr;
            } else {
                Logger::LogMessage(Severity::Info, "Warp adapter enumerated.");
            }

            hr = DXGIAdapter1.As(&DXGIAdapter4);
            if (FAILED(hr)) {
                Logger::LogMessage(Severity::Error, "Failed to get warp adapter.");
                return nullptr;
            } else {
                Logger::LogMessage(Severity::Info, "Warp adapter found.");
            }

            return DXGIAdapter4;
        } else {
            SIZE_T MaxDedicatedVideoMemory = 0;
            DXGIAdapter4 = nullptr;

            for (UINT Idx = 0; DXGIFactory->EnumAdapters1(Idx, &DXGIAdapter1) != DXGI_ERROR_NOT_FOUND; Idx++) {
                DXGI_ADAPTER_DESC1 DXGIAdapterDesc1{};
                DXGIAdapter1->GetDesc1(&DXGIAdapterDesc1);

                if ((DXGIAdapterDesc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0) {
                    // Test if this adapter can create a D3D12 device
                    if (SUCCEEDED(
                        D3D12CreateDevice(DXGIAdapter1.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr
                        ))) {
                        if (DXGIAdapterDesc1.DedicatedVideoMemory > MaxDedicatedVideoMemory) {
                            MaxDedicatedVideoMemory = DXGIAdapterDesc1.DedicatedVideoMemory;
                            HRESULT hr = DXGIAdapter1.As(&DXGIAdapter4);
                            if (FAILED(hr)) {
                                Logger::LogMessage(Severity::Error, "Failed to get IDXGIAdapter4 from IDXGIAdapter1.");
                            } else {
                                Logger::LogMessage(Severity::Info,
                                                   "Found suitable adapter with dedicated video memory.");
                            }
                        }
                    }
                }
            }

            if (!DXGIAdapter4) {
                // fallback to WARP if no suitable adapter found
                Logger::LogMessage(Severity::Warning, "No suitable hardware adapter found, falling back to WARP.");
                HRESULT hr = DXGIFactory->EnumWarpAdapter(IID_PPV_ARGS(&DXGIAdapter1));
                if (FAILED(hr)) {
                    Logger::LogMessage(Severity::Error, "Failed to enumerate warp adapter as fallback.");
                    return nullptr;
                }
                hr = DXGIAdapter1.As(&DXGIAdapter4);
                if (FAILED(hr)) {
                    Logger::LogMessage(Severity::Error, "Failed to get WARP adapter as fallback.");
                    return nullptr;
                }
            }

            return DXGIAdapter4;
        }
    }


    ComPtr<ID3D12Device10> Renderer::CreateDevice() {
        ComPtr<ID3D12Device10> Device{};

        HRESULT Hr = D3D12CreateDevice(m_DeviceContext->m_Adapter.Get(), D3D_FEATURE_LEVEL_12_2,IID_PPV_ARGS(&Device));
        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to create D3D12 device.");
        } else {
            Logger::LogMessage(Severity::Info, "D3D12 device created.");
        }

#ifdef _DEBUG
        ComPtr<ID3D12InfoQueue> InfoQueue;
        if (SUCCEEDED(Device.As(&InfoQueue))) {
            InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
            InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
            InfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

            // Suppress whole categories of messages
            //D3D12_MESSAGE_CATEGORY Categories[] = {};

            // Suppress messages based on their severity level
            D3D12_MESSAGE_SEVERITY Severities[] =
            {
                D3D12_MESSAGE_SEVERITY_INFO
            };

            // Suppress individual messages by their ID
            D3D12_MESSAGE_ID DenyIds[] = {
                D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
                D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,
                // This warning occurs when using capture frame while graphics debugging.
                D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE, // Same
            };

            D3D12_INFO_QUEUE_FILTER NewFilter = {};
            //NewFilter.DenyList.NumCategories = _countof(Categories);
            //NewFilter.DenyList.pCategoryList = Categories;
            NewFilter.DenyList.NumSeverities = _countof(Severities);
            NewFilter.DenyList.pSeverityList = Severities;
            NewFilter.DenyList.NumIDs = _countof(DenyIds);
            NewFilter.DenyList.pIDList = DenyIds;

            Hr = InfoQueue->PushStorageFilter(&NewFilter);
            if (FAILED(Hr)) {
                Logger::LogMessage(Severity::Error, "Failed to push storage filter.");
            } else {
                Logger::LogMessage(Severity::Info, "D3D12 info queue pushed.");
            }
        }
#endif


        return Device;
    }

    bool Renderer::CheckTearingSupport() {
        bool TearingSupport = false;

        UINT CreateFactoryFlags = 0;

#ifdef _DEBUG
        CreateFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

        ComPtr<IDXGIFactory7> Factory{};

        if (SUCCEEDED(CreateDXGIFactory2(CreateFactoryFlags,IID_PPV_ARGS(&Factory)))) {
            if (SUCCEEDED(
                Factory->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &TearingSupport, sizeof(TearingSupport)
                ))) {
                return true;
            }
        }
        return false;
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
        SwapChainDesc.Flags = CheckTearingSupport() ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

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
                GetWindowRect(Hwnd,&m_WindowRect);

                UINT WindowStyle = WS_OVERLAPPEDWINDOW & ~(WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX);

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


            }
            else {
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
        m_DeviceContext = std::make_unique<DeviceContext>();

        EnableDebugLayer();
        m_DeviceContext->m_bTearingSupported = CheckTearingSupport();

        m_DeviceContext->m_Adapter = QueryDx12Adapter();
        m_DeviceContext->m_Device = CreateDevice();
        m_CommandQueue = CreateCommandQueue(m_DeviceContext->m_Device, D3D12_COMMAND_LIST_TYPE_DIRECT);

        m_SwapChain = CreateSwapChain(Hwnd, m_CommandQueue, m_WindowWidth, m_WindowHeight, m_MaxFrames);

        m_CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();

        m_RTVDescriptorHeap = CreateDescriptorHeap(m_DeviceContext->m_Device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
                                                   m_MaxFrames);
        m_RTVDescriptorSize = m_DeviceContext->m_Device->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        UpdateRenderTarget(m_DeviceContext->m_Device, m_SwapChain, m_RTVDescriptorHeap);

        for (uint32_t i = 0; i < m_MaxFrames; i++) {
            m_CommandAllocators[i] = CreateCommandAllocator(m_DeviceContext->m_Device, D3D12_COMMAND_LIST_TYPE_DIRECT);
        }

        m_CommandList = CreateCommandList(m_DeviceContext->m_Device, m_CommandAllocators[m_CurrentBackBufferIndex],
                                          D3D12_COMMAND_LIST_TYPE_DIRECT);

        m_Fence = CreateFence(m_DeviceContext->m_Device);
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

            UINT SyncInterval = m_DeviceContext->m_bUseVSync ? 1 : 0;
            UINT PresentFlags = m_DeviceContext->m_bTearingSupported && !m_DeviceContext->m_bUseVSync
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
