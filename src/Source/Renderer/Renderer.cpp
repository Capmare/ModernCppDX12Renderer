//
// Created by david on 10/29/2025.
//


module;

#include <d3d12.h>
#include "../../d3dx12.h"
#include <DXGI.h>
#include <stdio.h>
#include <d3dcompiler.h>
#include <directxmath.h>


module HOX.Renderer;

import std;
import HOX.Types;
import HOX.Win32;
import HOX.Logger;
import HOX.Camera;
import HOX.CommandSystem;
import HOX.MemoryAllocator;
import HOX.DeviceManager;

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
                                                                u32 NumDescriptors) {
        ComPtr<ID3D12DescriptorHeap> DescriptorHeap{};

        D3D12_DESCRIPTOR_HEAP_DESC HeapDesc{};
        HeapDesc.Type = Type;
        HeapDesc.NumDescriptors = NumDescriptors;

        HRESULT Hr = Device->CreateDescriptorHeap(&HeapDesc,
                                                  HOX::Win32::UuidOf<ID3D12DescriptorHeap>(),
                                                  HOX::Win32::PpvArgs(DescriptorHeap.ReleaseAndGetAddressOf())
        );
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

        for (u32 i = 0; i < MaxFrames; i++) {
            ComPtr<ID3D12Resource2> BackBuffer{};
            HRESULT Hr = SwapChain->GetBuffer(i, HOX::Win32::UuidOf<ID3D12Resource2>(),
                                              HOX::Win32::PpvArgs(BackBuffer.ReleaseAndGetAddressOf()));
            if (FAILED(Hr)) {
                Logger::LogMessage(Severity::Error, "Failed to get back buffer.");
            } else {
                Logger::LogMessage(Severity::Info, "Getting back buffer.");
            }

            Device->CreateRenderTargetView(BackBuffer.Get(), nullptr, RtvHandle);
            m_SwapChain->UpdateBackBuffer(BackBuffer, i);

            RtvHandle.ptr += RTVDescriptorSize;
        }
    }

    void Renderer::CreateDepthBuffer(u32 Width, u32 Height) {

        HRESULT Hr{};

        auto LogD3DCompileFailure = [&](std::string_view what) {
            if (!FAILED(Hr)) return;

            std::string msg = "No error blob returned.";

            if (ErrorBlob && ErrorBlob->GetBufferPointer() && ErrorBlob->GetBufferSize() > 0) {
                const char *text = static_cast<const char *>(ErrorBlob->GetBufferPointer());
                const size_t len = ErrorBlob->GetBufferSize();
                msg.assign(text, text + len); // not null-terminated
            }

            Logger::LogMessage(
                Severity::Error,
                std::format("{} (HRESULT=0x{:08X})\n{}",
                            what, static_cast<unsigned>(Hr), msg)
            );
        };


        {
            m_DepthStencilBuffer.Reset();

            // DepthBuffer creation
            D3D12_RESOURCE_DESC DepthStencilDesc = {};
            DepthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; // 2d texture not a buffer
            DepthStencilDesc.Alignment = 0;
            DepthStencilDesc.Width = Width;
            DepthStencilDesc.Height = Height;
            DepthStencilDesc.DepthOrArraySize = 1;
            DepthStencilDesc.MipLevels = 1;
            DepthStencilDesc.Format = DXGI_FORMAT_D32_FLOAT; // 32 bit float depth
            DepthStencilDesc.SampleDesc.Count = 1;
            DepthStencilDesc.SampleDesc.Quality = 0;
            DepthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            DepthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // makes it a depth stencil

            D3D12_HEAP_PROPERTIES HeapProperties = {};
            HeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // GPU only memory

            D3D12_CLEAR_VALUE ClearValue = {};
            ClearValue.Format = DXGI_FORMAT_D32_FLOAT;
            ClearValue.DepthStencil.Depth = 1.0f;
            ClearValue.DepthStencil.Stencil = 0;

            Hr = GetDeviceContext().m_Device->CreateCommittedResource(
                &HeapProperties,
                D3D12_HEAP_FLAG_NONE,
                &DepthStencilDesc,
                D3D12_RESOURCE_STATE_DEPTH_WRITE,
                &ClearValue,
                HOX::Win32::UuidOf<ID3D12Resource>(),
                HOX::Win32::PpvArgs(m_DepthStencilBuffer.ReleaseAndGetAddressOf())
            );

            LogD3DCompileFailure("Failed to create depth stencil buffer ");
        }

        {
            D3D12_DESCRIPTOR_HEAP_DESC HeapDesc = {};
            HeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
            HeapDesc.NumDescriptors = 1; // One depth buffer
            HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // DSV heads are not shader visible

            GetDeviceContext().m_Device->CreateDescriptorHeap(
                &HeapDesc,
                HOX::Win32::UuidOf<ID3D12DescriptorHeap>(),
                HOX::Win32::PpvArgs(m_DSVHeap.ReleaseAndGetAddressOf())
            );

            LogD3DCompileFailure("Failed to create dsv descriptor ");

            D3D12_DEPTH_STENCIL_VIEW_DESC DepthStencilViewDesc = {};
            DepthStencilViewDesc.Format = DXGI_FORMAT_D32_FLOAT;
            DepthStencilViewDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
            DepthStencilViewDesc.Flags = D3D12_DSV_FLAG_NONE;
            DepthStencilViewDesc.Texture2D.MipSlice = 0;
            GetDeviceContext().m_Device->CreateDepthStencilView(
                m_DepthStencilBuffer.Get(),
                &DepthStencilViewDesc,
                m_DSVHeap->GetCPUDescriptorHandleForHeapStart());
        }
    }

    void Renderer::UpdateViewPortAndScissor(u32 Width, u32 Height) {
        m_Viewport.Width = Width;
        m_Viewport.Height = Height;
        m_ScissorRect.right = m_Viewport.Width;
        m_ScissorRect.bottom = m_Viewport.Height;
    }


    void Renderer::InitializeRenderer(HWND Hwnd) {
        GetDeviceContext().Hwnd = Hwnd;

        // Query adapter and create device
        m_DeviceManager = std::make_unique<DeviceManager>();
        m_DeviceManager->Initialize();

        m_bTearingSupported = m_DeviceManager->CheckTearingSupport();

        GetDeviceContext().m_CommandSystem = std::make_unique<HOX::CommandSystem>();
        GetDeviceContext().m_CommandSystem->Initialize();

        m_SwapChain = std::make_unique<Swapchain>();
        m_SwapChain->Initialize();

        m_RTVDescriptorHeap = CreateDescriptorHeap(GetDeviceContext().m_Device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
                                                   MaxFrames);
        m_RTVDescriptorSize = GetDeviceContext().m_Device->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        UpdateRenderTarget(GetDeviceContext().m_Device, m_SwapChain->GetSwapChain(), m_RTVDescriptorHeap);

        for (u32 i = 0; i < MaxFrames; i++) {
            m_CommandAllocators[i] = GetDeviceContext().m_CommandSystem->CreateCommandAllocator(
                D3D12_COMMAND_LIST_TYPE_DIRECT);
        }

        m_CommandList = GetDeviceContext().m_CommandSystem->CreateCommandList(GetDeviceContext().m_Device,
                                                                              m_CommandAllocators[m_SwapChain->
                                                                                  GetCurrentBackBufferIndex()],
                                                                              D3D12_COMMAND_LIST_TYPE_DIRECT);


        // Create fence and event handle
        m_Fence = std::make_unique<Fence>();
        if (!m_Fence) {
            Logger::LogMessage(Severity::Error, "Failed to create fence.");
        }


        ResizeSwapChain(GetDeviceContext().m_WindowWidth, GetDeviceContext().m_WindowHeight);


        DeviceManager::PrintDebugMessages(GetDeviceContext().m_Device.Get());
        
        {
            GetDeviceContext().m_Allocator = std::make_unique<HOX::MemoryAllocator>();
            GetDeviceContext().m_Allocator->Initialize(GetDeviceContext().m_Device.Get(),
                                                       GetDeviceContext().m_Adapter.Get());
        }

        // setting up triangle
        Vertex TriangleVertices[] = {
            // First triangle (front, Z=0)
            {{0.0f, 0.5f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
            {{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
            {{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},

            // Second triangle (behind, Z=0.5)
            {{0.2f, 0.7f, 0.5f}, {1.0f, 1.0f, 0.0f, 1.0f}},
            {{0.7f, -0.3f, 0.5f}, {1.0f, 1.0f, 0.0f, 1.0f}},
            {{-0.3f, -0.3f, 0.5f}, {1.0f, 1.0f, 0.0f, 1.0f}},
        };

        HRESULT Hr{};

        {
            D3D12_HEAP_PROPERTIES HeapProps = {};
            HeapProps.Type = D3D12_HEAP_TYPE_UPLOAD; // cpu can write gpu can read
            HeapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
            HeapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

            D3D12_RESOURCE_DESC ResourceDesc = {};
            ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            ResourceDesc.Alignment = 0;
            ResourceDesc.Width = sizeof(TriangleVertices);
            ResourceDesc.Height = 1;
            ResourceDesc.DepthOrArraySize = 1;
            ResourceDesc.MipLevels = 1;
            ResourceDesc.MipLevels = 1;
            ResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
            ResourceDesc.SampleDesc.Count = 1;
            ResourceDesc.SampleDesc.Quality = 0;
            ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            ResourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

            Hr = GetDeviceContext().m_Device->CreateCommittedResource(
                &HeapProps,
                D3D12_HEAP_FLAG_NONE,
                &ResourceDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                HOX::Win32::UuidOf<ID3D12Resource>(), HOX::Win32::PpvArgs(m_VertexBuffer.ReleaseAndGetAddressOf()));
        }

        void *pData = nullptr;
        D3D12_RANGE ReadRange = {0, 0};

        m_VertexBuffer->Map(0, &ReadRange, &pData);
        memcpy(pData, &TriangleVertices, sizeof(TriangleVertices));

        m_VertexBuffer->Unmap(0, nullptr);

        m_VertexBufferView.BufferLocation = m_VertexBuffer->GetGPUVirtualAddress();
        m_VertexBufferView.SizeInBytes = sizeof(TriangleVertices);
        m_VertexBufferView.StrideInBytes = sizeof(Vertex);

        // shader stage

        auto LogD3DCompileFailure = [&](std::string_view what) {
            if (!FAILED(Hr)) return;

            std::string msg = "No error blob returned.";

            if (ErrorBlob && ErrorBlob->GetBufferPointer() && ErrorBlob->GetBufferSize() > 0) {
                const char *text = static_cast<const char *>(ErrorBlob->GetBufferPointer());
                const size_t len = ErrorBlob->GetBufferSize();
                msg.assign(text, text + len); // not null-terminated
            }

            Logger::LogMessage(
                Severity::Error,
                std::format("{} (HRESULT=0x{:08X})\n{}",
                            what, static_cast<unsigned>(Hr), msg)
            );
        };


        Hr = D3DCompileFromFile(
            L"Shaders/VertexShader.hlsl",
            nullptr,
            nullptr,
            "main", // entry point
            "vs_5_0", // vs = vertex shader, 5_0 shader model
            D3DCOMPILE_DEBUG, // should remove this in non debug configs
            0,
            &VertexShaderBlob,
            &ErrorBlob
        );

        LogD3DCompileFailure("Failed to compile vertex shader.");

        Hr = D3DCompileFromFile(
            L"Shaders/PixelShader.hlsl",
            nullptr,
            nullptr,
            "main", // entry point
            "ps_5_0", // vs = vertex shader, 5_0 shader model
            D3DCOMPILE_DEBUG, // should remove this in non debug configs
            0,
            &PixelShaderBlob,
            &ErrorBlob
        );

        LogD3DCompileFailure("Failed to compile Pixel shader.");


        // CAMERA
        {
            m_Camera = std::make_unique<Camera>();
            float AspectRatio = static_cast<float>(GetDeviceContext().m_WindowWidth) /
                    static_cast<float>(GetDeviceContext().m_WindowHeight);
            m_Camera->UpdateAspectRatio(AspectRatio);

            D3D12_HEAP_PROPERTIES HeapProps = {};
            HeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

            D3D12_RESOURCE_DESC ResourceDesc = {};
            ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            ResourceDesc.Width = HOX::CameraConstantsSize;
            ResourceDesc.Height = 1;
            ResourceDesc.DepthOrArraySize = 1;
            ResourceDesc.MipLevels = 1;
            ResourceDesc.Format = DXGI_FORMAT_UNKNOWN;
            ResourceDesc.SampleDesc.Count = 1;
            ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

            GetDeviceContext().m_Device->CreateCommittedResource(
                &HeapProps,
                D3D12_HEAP_FLAG_NONE,
                &ResourceDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                HOX::Win32::UuidOf<ID3D12Resource>(),
                HOX::Win32::PpvArgs(m_CameraConstantbuffer.ReleaseAndGetAddressOf()));

            D3D12_RANGE ReadRange = {0, 0}; // no read
            m_CameraConstantbuffer->Map(0, &ReadRange, &m_CameraConstantBufferMapped);

        }

        // Root parameter for constant buffer
        D3D12_ROOT_PARAMETER RootParameter = {};
        RootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // constat buffer view
        RootParameter.Descriptor.ShaderRegister = 0; // register b0
        RootParameter.Descriptor.RegisterSpace = 0;
        RootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

        // root signatures
        D3D12_ROOT_SIGNATURE_DESC RootSignatureDesc = {};
        RootSignatureDesc.NumParameters = 1;
        RootSignatureDesc.pParameters = &RootParameter;
        RootSignatureDesc.NumStaticSamplers = 0;
        RootSignatureDesc.pStaticSamplers = nullptr;
        RootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        Hr = D3D12SerializeRootSignature(
            &RootSignatureDesc,
            D3D_ROOT_SIGNATURE_VERSION_1,
            &SignatureBlob,
            &ErrorBlob
        );

        LogD3DCompileFailure("Failed to serialize root signature ");

        Hr = GetDeviceContext().m_Device->CreateRootSignature(
            0,
            SignatureBlob->GetBufferPointer(),
            SignatureBlob->GetBufferSize(),
            HOX::Win32::UuidOf<ID3D12RootSignature>(),
            HOX::Win32::PpvArgs(m_RootSignature.ReleaseAndGetAddressOf())
        );

        LogD3DCompileFailure("Failed to make root signature ");

        // Making the PSO

        D3D12_GRAPHICS_PIPELINE_STATE_DESC GraphicsPipelineDesc = {};
        GraphicsPipelineDesc.pRootSignature = m_RootSignature.Get();
        GraphicsPipelineDesc.VS = {
            VertexShaderBlob->GetBufferPointer(),
            VertexShaderBlob->GetBufferSize(),
        };
        GraphicsPipelineDesc.PS = {
            PixelShaderBlob->GetBufferPointer(),
            PixelShaderBlob->GetBufferSize(),
        };

        // Connecting c++ vertex struct to the shader input

        D3D12_INPUT_ELEMENT_DESC InputLayoutDesc[] = {
            {
                "POSITION", // Name in shader
                0, // Position 0
                DXGI_FORMAT_R32G32B32_FLOAT, // float3
                0, // INPUT SLOT
                0, // BYTE OFFSET FROM START OF VERTEX
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                0
            },
            {
                "COLOR",
                0,
                DXGI_FORMAT_R32G32B32A32_FLOAT, // float4
                0,
                12, // 3* 4 Offset
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                0
            }
        };

        const int WindowWidth = static_cast<int>(GetDeviceContext().m_WindowWidth);
        const int WindowHeight = static_cast<int>(GetDeviceContext().m_WindowHeight);
        CreateDepthBuffer(WindowWidth, WindowHeight);


        GraphicsPipelineDesc.InputLayout = {InputLayoutDesc, _countof(InputLayoutDesc)};
        GraphicsPipelineDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        // Fillmode = Solid not wireframe; CullMode = Back; ClockWise = Front;

        GraphicsPipelineDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        // No blending just overwrite pixels, will need to change for transparency

        // Disable depth , no depth buffer yet
        GraphicsPipelineDesc.DepthStencilState.DepthEnable = true;
        GraphicsPipelineDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        GraphicsPipelineDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        GraphicsPipelineDesc.DepthStencilState.StencilEnable = false;
        GraphicsPipelineDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT; // same as depth buffer

        GraphicsPipelineDesc.SampleMask = UINT_MAX; // No multisampling mask
        GraphicsPipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; // Drawing triangles
        GraphicsPipelineDesc.NumRenderTargets = 1; // One render target
        GraphicsPipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM; // Must match swapchain format
        GraphicsPipelineDesc.SampleDesc.Count = 1; // No MSAA (yet)
        GraphicsPipelineDesc.SampleDesc.Quality = 0;

        Hr = GetDeviceContext().m_Device->CreateGraphicsPipelineState(
            &GraphicsPipelineDesc,
            HOX::Win32::UuidOf<ID3D12PipelineState>(),
            HOX::Win32::PpvArgs(m_PipelineState.ReleaseAndGetAddressOf())
        );
        LogD3DCompileFailure("Failed to create graphics pipeline ");


        m_Viewport.TopLeftX = 0.0f;
        m_Viewport.TopLeftY = 0.0f;
        m_Viewport.Width = static_cast<float>(GetDeviceContext().m_WindowWidth);
        m_Viewport.Height = static_cast<float>(GetDeviceContext().m_WindowHeight);
        m_Viewport.MinDepth = 0.0f;
        m_Viewport.MaxDepth = 1.0f;

        m_ScissorRect.left = 0.0f;
        m_ScissorRect.top = 0.0f;
        m_ScissorRect.right = static_cast<float>(GetDeviceContext().m_WindowWidth);
        m_ScissorRect.bottom = static_cast<float>(GetDeviceContext().m_WindowHeight);
    };


    void Renderer::Render() {
        // Get current frame index from the swap chain
        auto CurrentBackBufferIndex = m_SwapChain->GetCurrentBackBufferIndex();

        // Grab the command allocator and back buffer for this frame
        auto CommandAllocator = m_CommandAllocators[CurrentBackBufferIndex];
        auto BackBuffer = m_SwapChain->GetBackBuffer(CurrentBackBufferIndex);

        // Safety checks
        if (!BackBuffer) {
            Logger::LogMessage(Severity::Error,
                               "BackBuffer is null at frame index " + std::to_string(CurrentBackBufferIndex));
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
        } {
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

            D3D12_CPU_DESCRIPTOR_HANDLE DSV
            {
                m_DSVHeap->GetCPUDescriptorHandleForHeapStart()
            };

            m_CommandList->ClearDepthStencilView(DSV, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);


            m_CommandList->OMSetRenderTargets(1, &RTV, FALSE, &DSV);
            m_CommandList->ClearRenderTargetView(RTV, clearColor, 0, nullptr);
        }




        // Draw
        {
            m_CommandList->SetGraphicsRootSignature(m_RootSignature.Get());
            m_CommandList->SetPipelineState(m_PipelineState.Get());

            // Camera movement && and binding
            {
                const DirectX::XMMATRIX ViewProjection = m_Camera->GetViewProjectionMatrix();

                CameraConstants Constants;
                DirectX::XMStoreFloat4x4(&Constants.m_ViewProjection, ViewProjection);
                memcpy(m_CameraConstantBufferMapped, &Constants, sizeof(Constants));

                m_CommandList->SetGraphicsRootConstantBufferView(0,
                    m_CameraConstantbuffer->GetGPUVirtualAddress());
            }


            m_CommandList->IASetVertexBuffers(0, 1, &m_VertexBufferView);
            m_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            m_CommandList->RSSetViewports(1, &m_Viewport);
            m_CommandList->RSSetScissorRects(1, &m_ScissorRect);
            m_CommandList->DrawInstanced(6, 1, 0, 0);
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
        static u64 frameCounter = 0;
        static double elapsedSeconds = 0.0;
        static std::chrono::high_resolution_clock clock;
        static auto t0 = clock.now();

        frameCounter++;
        auto t1 = clock.now();
        auto deltaTime = (t1 - t0).count()  * 1e-9f;
        t0 = t1;

        elapsedSeconds += deltaTime;
        if (elapsedSeconds > 1.0) {
            char buffer[500];
            auto fps = frameCounter / elapsedSeconds;
            sprintf_s(buffer, 500, "FPS: %f\n", fps);
            OutputDebugString(buffer);

            frameCounter = 0;
            elapsedSeconds = 0.0;
        }

       m_Camera->Update(deltaTime);
    }

    void Renderer::CleanUpRenderer() {
        GetDeviceContext().m_CommandSystem->FlushCommands(m_Fence->GetFence(), m_Fence->GetFenceValue(),
                                                          m_Fence->GetFenceEvent());
        CloseHandle(m_Fence->GetFenceEvent());
    }

    void Renderer::ResizeSwapChain(const u32 Width, const u32 Height) {
        if (!m_SwapChain) {
            Logger::LogMessage(Severity::ErrorNoCrash, "Failed to resize window due to swapchain.");
            return;
        }
        if (!m_Fence) {
            Logger::LogMessage(Severity::ErrorNoCrash, "Failed to resize window due to fence.");
            return;
        }

        m_SwapChain->Resize(m_Fence.get(), Width, Height);
        UpdateRenderTarget(GetDeviceContext().m_Device, m_SwapChain->GetSwapChain(), m_RTVDescriptorHeap);

        CreateDepthBuffer(Width, Height);
        UpdateViewPortAndScissor(Width, Height);

        if (m_Camera) {
            float AspectRatio = Width / static_cast<float>(Height);
            m_Camera->UpdateAspectRatio(AspectRatio);
        }

    }
} // HOX
