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
import HOX.Scene;
import HOX.ModelLoader;
import HOX.GameObject;
import HOX.Mesh;
import HOX.LightTypes;
import HOX.CascadedShadowMap;
import HOX.SSAO;

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

            if (m_ErrorBlob && m_ErrorBlob->GetBufferPointer() && m_ErrorBlob->GetBufferSize() > 0) {
                const char *text = static_cast<const char *>(m_ErrorBlob->GetBufferPointer());
                const size_t len = m_ErrorBlob->GetBufferSize();
                msg.assign(text, text + len); // not null-terminated
            }

            Logger::LogMessage(
                Severity::Error,
                std::format("{} (HRESULT=0x{:08X})\n{}",
                            what, static_cast<unsigned>(Hr), msg)
            );
        }; {
            m_DepthStencilBuffer.Reset();

            // DepthBuffer creation
            D3D12_RESOURCE_DESC DepthStencilDesc = {};
            DepthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; // 2d texture not a buffer
            DepthStencilDesc.Alignment = 0;
            DepthStencilDesc.Width = Width;
            DepthStencilDesc.Height = Height;
            DepthStencilDesc.DepthOrArraySize = 1;
            DepthStencilDesc.MipLevels = 1;
            DepthStencilDesc.Format = DXGI_FORMAT_R32_TYPELESS; // Typeless to allow both DSV and SRV
            DepthStencilDesc.SampleDesc.Count = 1;
            DepthStencilDesc.SampleDesc.Quality = 0;
            DepthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
            DepthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // makes it a depth stencil

            D3D12_HEAP_PROPERTIES HeapProperties = {};
            HeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // GPU only memory

            D3D12_CLEAR_VALUE ClearValue = {};
            ClearValue.Format = DXGI_FORMAT_D32_FLOAT; // Clear value still uses typed format
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

        {
            D3D12_SHADER_RESOURCE_VIEW_DESC DepthSRVDesc = {};
            DepthSRVDesc.Format = DXGI_FORMAT_R32_FLOAT;  // Read as float
            DepthSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
            DepthSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            DepthSRVDesc.Texture2D.MipLevels = 1;

            m_DepthBufferSRVIndex = m_SRVHeap->Allocate();
            GetDeviceContext().m_Device->CreateShaderResourceView(
                m_DepthStencilBuffer.Get(),
                &DepthSRVDesc,
                m_SRVHeap->GetCPUHandle(m_DepthBufferSRVIndex));
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


        DeviceManager::PrintDebugMessages(GetDeviceContext().m_Device.Get()); {
            GetDeviceContext().m_Allocator = std::make_unique<HOX::MemoryAllocator>();
            GetDeviceContext().m_Allocator->Initialize(GetDeviceContext().m_Device.Get(),
                                                       GetDeviceContext().m_Adapter.Get());
        } {
            m_SRVHeap = std::make_unique<DescriptorHeap>();
            m_SRVHeap->Initialize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);
        }

        // ResizeSwapChain needs m_SRVHeap to be initialized first (for depth buffer SRV)
        ResizeSwapChain(GetDeviceContext().m_WindowWidth, GetDeviceContext().m_WindowHeight);

        m_CommandAllocators[0]->Reset();
        m_CommandList->Reset(m_CommandAllocators[0].Get(), nullptr);

        // Default textures for PBR
        {
            // Default 1x1 Magenta texture for missing albedo
            m_DefaultTexture = std::make_unique<Texture>();
            unsigned char MagentaPixel[4] = {255, 0, 255, 255};
            m_DefaultTexture->CreateFromPixels(MagentaPixel, 1, 1, m_CommandList.Get());
            m_DefaultTexture->CreateSRV(m_SRVHeap.get());

            // Default 1x1 flat normal map (128, 128, 255, 255) = (0.5, 0.5, 1.0) in [0,1] = (0, 0, 1) in [-1,1]
            m_DefaultNormalMap = std::make_unique<Texture>();
            unsigned char FlatNormalPixel[4] = {128, 128, 255, 255};
            m_DefaultNormalMap->CreateFromPixels(FlatNormalPixel, 1, 1, m_CommandList.Get());
            m_DefaultNormalMap->CreateSRV(m_SRVHeap.get());

            // Default metallic-roughness (non-metallic, medium roughness)
            // glTF: G = roughness, B = metallic
            m_DefaultMetallicRoughness = std::make_unique<Texture>();
            unsigned char DefaultMRPixel[4] = {0, 128, 0, 255}; // roughness=0.5, metallic=0
            m_DefaultMetallicRoughness->CreateFromPixels(DefaultMRPixel, 1, 1, m_CommandList.Get());
            m_DefaultMetallicRoughness->CreateSRV(m_SRVHeap.get());

            GetDeviceContext().m_CommandSystem->ExecuteAndFlush(
                m_CommandList.Get(),
                m_CommandAllocators[0].Get(),
                m_Fence->GetFence().Get(),
                m_Fence->GetFenceValue(),
                m_Fence->GetFenceEvent());
            m_CommandList->Close();
        }

        HRESULT Hr{};

        // shader stage

        auto LogD3DCompileFailure = [&](std::string_view what) {
            if (!FAILED(Hr)) return;

            std::string msg = "No error blob returned.";

            if (m_ErrorBlob && m_ErrorBlob->GetBufferPointer() && m_ErrorBlob->GetBufferSize() > 0) {
                const char *text = static_cast<const char *>(m_ErrorBlob->GetBufferPointer());
                const size_t len = m_ErrorBlob->GetBufferSize();
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
            &m_VertexShaderBlob,
            &m_ErrorBlob
        );

        LogD3DCompileFailure("Failed to compile vertex shader.");

        Hr = D3DCompileFromFile(
            L"Shaders/ForwardPlusPS.hlsl",
            nullptr,
            nullptr,
            "main",
            "ps_5_0",
            D3DCOMPILE_DEBUG,
            0,
            &m_PixelShaderBlob,
            &m_ErrorBlob
        );

        LogD3DCompileFailure("Failed to compile Forward+ Pixel shader.");

        Hr = D3DCompileFromFile(
            L"Shaders/DepthPassVS.hlsl",
            nullptr,
            nullptr,
            "main", // entry point
            "vs_5_0", // vs = vertex shader, 5_0 shader model
            D3DCOMPILE_DEBUG, // should remove this in non debug configs
            0,
            &m_DepthVertexShaderShaderBlob,
            &m_ErrorBlob
        );

        LogD3DCompileFailure("Failed to compile DepthPass vertex shader.");

        // Depth Pass Pixel Shader (for alpha testing)
        Hr = D3DCompileFromFile(
            L"Shaders/DepthPS.hlsl",
            nullptr,
            nullptr,
            "main",
            "ps_5_0",
            D3DCOMPILE_DEBUG,
            0,
            &m_DepthPixelShaderBlob,
            &m_ErrorBlob);

        LogD3DCompileFailure("Failed to compile DepthPass pixel shader.");

        Hr = D3DCompileFromFile(
            L"Shaders/LightCullingCS.hlsl",
            nullptr,
            nullptr,
            "main",
            "cs_5_0",
            D3DCOMPILE_DEBUG,
            0,
            &m_LightCullingCSBlob,
            &m_ErrorBlob);

        LogD3DCompileFailure("Failed to compile LightCulling CS.");

        // Shadow Map Vertex Shader
        Hr = D3DCompileFromFile(
            L"Shaders/ShadowMapVS.hlsl",
            nullptr,
            nullptr,
            "main",
            "vs_5_0",
            D3DCOMPILE_DEBUG,
            0,
            &m_ShadowVSBlob,
            &m_ErrorBlob);

        LogD3DCompileFailure("Failed to compile ShadowMap VS.");

        // Shadow Map Pixel Shader (for alpha testing)
        Hr = D3DCompileFromFile(
            L"Shaders/ShadowMapPS.hlsl",
            nullptr,
            nullptr,
            "main",
            "ps_5_0",
            D3DCOMPILE_DEBUG,
            0,
            &m_ShadowPSBlob,
            &m_ErrorBlob);

        LogD3DCompileFailure("Failed to compile ShadowMap PS.");

        // SSAO Compute Shader
        Hr = D3DCompileFromFile(
            L"Shaders/SSAOCS.hlsl",
            nullptr,
            nullptr,
            "main",
            "cs_5_0",
            D3DCOMPILE_DEBUG,
            0,
            &m_SSAOCSBlob,
            &m_ErrorBlob);

        LogD3DCompileFailure("Failed to compile SSAO CS.");

        // SSAO Blur Compute Shader
        Hr = D3DCompileFromFile(
            L"Shaders/SSAOBlurCS.hlsl",
            nullptr,
            nullptr,
            "main",
            "cs_5_0",
            D3DCOMPILE_DEBUG,
            0,
            &m_SSAOBlurCSBlob,
            &m_ErrorBlob);

        LogD3DCompileFailure("Failed to compile SSAO Blur CS.");

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

        // Graphics root signature for Forward+ PBR rendering
        {
            // Descriptor ranges for SRVs (each needs its own range since they're not contiguous)
            D3D12_DESCRIPTOR_RANGE AlbedoSRVRange = {};
            AlbedoSRVRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            AlbedoSRVRange.NumDescriptors = 1;
            AlbedoSRVRange.BaseShaderRegister = 0; // t0 - albedo
            AlbedoSRVRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

            D3D12_DESCRIPTOR_RANGE NormalMapSRVRange = {};
            NormalMapSRVRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            NormalMapSRVRange.NumDescriptors = 1;
            NormalMapSRVRange.BaseShaderRegister = 1; // t1 - normal map
            NormalMapSRVRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

            D3D12_DESCRIPTOR_RANGE MetallicRoughnessSRVRange = {};
            MetallicRoughnessSRVRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            MetallicRoughnessSRVRange.NumDescriptors = 1;
            MetallicRoughnessSRVRange.BaseShaderRegister = 2; // t2 - metallic-roughness
            MetallicRoughnessSRVRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

            D3D12_DESCRIPTOR_RANGE LightsSRVRange = {};
            LightsSRVRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            LightsSRVRange.NumDescriptors = 1;
            LightsSRVRange.BaseShaderRegister = 3; // t3 - lights
            LightsSRVRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

            D3D12_DESCRIPTOR_RANGE LightGridSRVRange = {};
            LightGridSRVRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            LightGridSRVRange.NumDescriptors = 1;
            LightGridSRVRange.BaseShaderRegister = 4; // t4 - light grid
            LightGridSRVRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

            D3D12_DESCRIPTOR_RANGE LightIndexListSRVRange = {};
            LightIndexListSRVRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            LightIndexListSRVRange.NumDescriptors = 1;
            LightIndexListSRVRange.BaseShaderRegister = 5; // t5 - light index list
            LightIndexListSRVRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

            // NEW: Shadow map SRV range (Texture2DArray with 4 cascades)
            D3D12_DESCRIPTOR_RANGE ShadowMapSRVRange = {};
            ShadowMapSRVRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            ShadowMapSRVRange.NumDescriptors = 1;
            ShadowMapSRVRange.BaseShaderRegister = 6; // t6 - shadow map array
            ShadowMapSRVRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

            // NEW: SSAO SRV range
            D3D12_DESCRIPTOR_RANGE SSAOSRVRange = {};
            SSAOSRVRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            SSAOSRVRange.NumDescriptors = 1;
            SSAOSRVRange.BaseShaderRegister = 7; // t7 - SSAO texture
            SSAOSRVRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

            D3D12_ROOT_PARAMETER RootParameter[13] = {};

            // 0: Camera CBV (b0) - vertex + pixel
            RootParameter[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
            RootParameter[0].Descriptor.ShaderRegister = 0;
            RootParameter[0].Descriptor.RegisterSpace = 0;
            RootParameter[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

            // 1: Object CBV (b1) - vertex only
            RootParameter[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
            RootParameter[1].Descriptor.ShaderRegister = 1;
            RootParameter[1].Descriptor.RegisterSpace = 0;
            RootParameter[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

            // 2: Albedo texture SRV table (t0) - pixel only
            RootParameter[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            RootParameter[2].DescriptorTable.NumDescriptorRanges = 1;
            RootParameter[2].DescriptorTable.pDescriptorRanges = &AlbedoSRVRange;
            RootParameter[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

            // 3: Normal map SRV table (t1) - pixel only
            RootParameter[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            RootParameter[3].DescriptorTable.NumDescriptorRanges = 1;
            RootParameter[3].DescriptorTable.pDescriptorRanges = &NormalMapSRVRange;
            RootParameter[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

            // 4: Metallic-roughness SRV table (t2) - pixel only
            RootParameter[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            RootParameter[4].DescriptorTable.NumDescriptorRanges = 1;
            RootParameter[4].DescriptorTable.pDescriptorRanges = &MetallicRoughnessSRVRange;
            RootParameter[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

            // 5: Lights SRV table (t3) - pixel only
            RootParameter[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            RootParameter[5].DescriptorTable.NumDescriptorRanges = 1;
            RootParameter[5].DescriptorTable.pDescriptorRanges = &LightsSRVRange;
            RootParameter[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

            // 6: Light Grid SRV table (t4) - pixel only
            RootParameter[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            RootParameter[6].DescriptorTable.NumDescriptorRanges = 1;
            RootParameter[6].DescriptorTable.pDescriptorRanges = &LightGridSRVRange;
            RootParameter[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

            // 7: Light Index List SRV table (t5) - pixel only
            RootParameter[7].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            RootParameter[7].DescriptorTable.NumDescriptorRanges = 1;
            RootParameter[7].DescriptorTable.pDescriptorRanges = &LightIndexListSRVRange;
            RootParameter[7].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

            // 8: Screen constants (b2) - pixel only (using root constants for simplicity)
            RootParameter[8].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
            RootParameter[8].Constants.ShaderRegister = 2;
            RootParameter[8].Constants.RegisterSpace = 0;
            RootParameter[8].Constants.Num32BitValues = 4; // ScreenWidth, ScreenHeight, TileCountX, TileCountY
            RootParameter[8].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

            // 9: Shadow Map SRV table (t6) - pixel only
            RootParameter[9].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            RootParameter[9].DescriptorTable.NumDescriptorRanges = 1;
            RootParameter[9].DescriptorTable.pDescriptorRanges = &ShadowMapSRVRange;
            RootParameter[9].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

            // 10: SSAO SRV table (t7) - pixel only
            RootParameter[10].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            RootParameter[10].DescriptorTable.NumDescriptorRanges = 1;
            RootParameter[10].DescriptorTable.pDescriptorRanges = &SSAOSRVRange;
            RootParameter[10].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

            // 11: Shadow CBV (b3) - pixel only
            RootParameter[11].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
            RootParameter[11].Descriptor.ShaderRegister = 3;
            RootParameter[11].Descriptor.RegisterSpace = 0;
            RootParameter[11].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

            // 12: Tone Mapping Constants (b4) - pixel only
            RootParameter[12].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
            RootParameter[12].Descriptor.ShaderRegister = 4;
            RootParameter[12].Descriptor.RegisterSpace = 0;
            RootParameter[12].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

            // Static samplers
            D3D12_STATIC_SAMPLER_DESC StaticSamplers[2] = {};

            // s0: Standard texture sampler
            StaticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            StaticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            StaticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            StaticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            StaticSamplers[0].MipLODBias = 0.0f;
            StaticSamplers[0].MaxAnisotropy = 1;
            StaticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
            StaticSamplers[0].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
            StaticSamplers[0].MinLOD = 0.0f;
            StaticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
            StaticSamplers[0].ShaderRegister = 0; // s0
            StaticSamplers[0].RegisterSpace = 0;
            StaticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

            // s1: Shadow comparison sampler (PCF)
            StaticSamplers[1].Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
            StaticSamplers[1].AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
            StaticSamplers[1].AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
            StaticSamplers[1].AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
            StaticSamplers[1].MipLODBias = 0.0f;
            StaticSamplers[1].MaxAnisotropy = 1;
            StaticSamplers[1].ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
            StaticSamplers[1].BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
            StaticSamplers[1].MinLOD = 0.0f;
            StaticSamplers[1].MaxLOD = D3D12_FLOAT32_MAX;
            StaticSamplers[1].ShaderRegister = 1; // s1
            StaticSamplers[1].RegisterSpace = 0;
            StaticSamplers[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

            D3D12_ROOT_SIGNATURE_DESC RootSignatureDesc = {};
            RootSignatureDesc.NumParameters = 13;
            RootSignatureDesc.pParameters = RootParameter;
            RootSignatureDesc.NumStaticSamplers = 2;
            RootSignatureDesc.pStaticSamplers = StaticSamplers;
            RootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

            Hr = D3D12SerializeRootSignature(
                &RootSignatureDesc,
                D3D_ROOT_SIGNATURE_VERSION_1,
                &SignatureBlob,
                &m_ErrorBlob
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
        }

        // Compute root signature for light culling
        {
            // Use individual descriptor tables for each SRV since they aren't contiguous
            D3D12_DESCRIPTOR_RANGE LightsSRVRange = {};
            LightsSRVRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            LightsSRVRange.NumDescriptors = 1;
            LightsSRVRange.BaseShaderRegister = 0; // t0
            LightsSRVRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

            D3D12_DESCRIPTOR_RANGE DepthSRVRange = {};
            DepthSRVRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            DepthSRVRange.NumDescriptors = 1;
            DepthSRVRange.BaseShaderRegister = 1; // t1
            DepthSRVRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

            D3D12_DESCRIPTOR_RANGE LightGridUAVRange = {};
            LightGridUAVRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
            LightGridUAVRange.NumDescriptors = 1;
            LightGridUAVRange.BaseShaderRegister = 0; // u0
            LightGridUAVRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

            D3D12_DESCRIPTOR_RANGE LightIndexListUAVRange = {};
            LightIndexListUAVRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
            LightIndexListUAVRange.NumDescriptors = 1;
            LightIndexListUAVRange.BaseShaderRegister = 1; // u1
            LightIndexListUAVRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

            D3D12_DESCRIPTOR_RANGE CounterUAVRange = {};
            CounterUAVRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
            CounterUAVRange.NumDescriptors = 1;
            CounterUAVRange.BaseShaderRegister = 2; // u2
            CounterUAVRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

            D3D12_ROOT_PARAMETER RootParameter[6] = {};

            // 0: CBV for culling constants (b0)
            RootParameter[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
            RootParameter[0].Descriptor.ShaderRegister = 0;
            RootParameter[0].Descriptor.RegisterSpace = 0;
            RootParameter[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

            // 1: Lights SRV table (t0)
            RootParameter[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            RootParameter[1].DescriptorTable.NumDescriptorRanges = 1;
            RootParameter[1].DescriptorTable.pDescriptorRanges = &LightsSRVRange;
            RootParameter[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

            // 2: Depth SRV table (t1)
            RootParameter[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            RootParameter[2].DescriptorTable.NumDescriptorRanges = 1;
            RootParameter[2].DescriptorTable.pDescriptorRanges = &DepthSRVRange;
            RootParameter[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

            // 3: Light Grid UAV table (u0)
            RootParameter[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            RootParameter[3].DescriptorTable.NumDescriptorRanges = 1;
            RootParameter[3].DescriptorTable.pDescriptorRanges = &LightGridUAVRange;
            RootParameter[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

            // 4: Light Index List UAV table (u1)
            RootParameter[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            RootParameter[4].DescriptorTable.NumDescriptorRanges = 1;
            RootParameter[4].DescriptorTable.pDescriptorRanges = &LightIndexListUAVRange;
            RootParameter[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

            // 5: Counter UAV table (u2)
            RootParameter[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            RootParameter[5].DescriptorTable.NumDescriptorRanges = 1;
            RootParameter[5].DescriptorTable.pDescriptorRanges = &CounterUAVRange;
            RootParameter[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

            D3D12_ROOT_SIGNATURE_DESC ComputeRootSigDesc = {};
            ComputeRootSigDesc.NumParameters = 6;
            ComputeRootSigDesc.pParameters = RootParameter;
            ComputeRootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

            ComPtr<ID3DBlob> ComputeSigBlob;
            Hr = D3D12SerializeRootSignature(&ComputeRootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, &ComputeSigBlob, &m_ErrorBlob);
            LogD3DCompileFailure("Failed to serialize compute root signature.");

            Hr = GetDeviceContext().m_Device->CreateRootSignature(
                0,
                ComputeSigBlob->GetBufferPointer(),
                ComputeSigBlob->GetBufferSize(),
                HOX::Win32::UuidOf<ID3D12RootSignature>(),
                HOX::Win32::PpvArgs(m_ComputeRootSignature.ReleaseAndGetAddressOf()));
            LogD3DCompileFailure("Failed to create compute root signature.");
        }



        // Making the PSO

        // Light culling compute PSO
        {
            D3D12_COMPUTE_PIPELINE_STATE_DESC ComputePSODesc = {};
            ComputePSODesc.pRootSignature = m_ComputeRootSignature.Get();
            ComputePSODesc.CS = {
                m_LightCullingCSBlob->GetBufferPointer(),
                m_LightCullingCSBlob->GetBufferSize()
            };

            Hr = GetDeviceContext().m_Device->CreateComputePipelineState(
                &ComputePSODesc,
                HOX::Win32::UuidOf<ID3D12PipelineState>(),
                HOX::Win32::PpvArgs(m_LightCullingPipelineState.ReleaseAndGetAddressOf()));
            LogD3DCompileFailure("Failed to create light culling compute PSO.");
        }

        // Shadow Map Root Signature (with texture for alpha testing)
        {
            // Descriptor range for albedo texture (t0)
            D3D12_DESCRIPTOR_RANGE AlbedoSRVRange = {};
            AlbedoSRVRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            AlbedoSRVRange.NumDescriptors = 1;
            AlbedoSRVRange.BaseShaderRegister = 0; // t0
            AlbedoSRVRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

            D3D12_ROOT_PARAMETER ShadowRootParams[3] = {};

            // 0: Root constants for light view-projection (b0) - 16 floats = 64 bytes
            ShadowRootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
            ShadowRootParams[0].Constants.ShaderRegister = 0;
            ShadowRootParams[0].Constants.RegisterSpace = 0;
            ShadowRootParams[0].Constants.Num32BitValues = 16; // 4x4 matrix
            ShadowRootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

            // 1: CBV for object world matrix (b1)
            ShadowRootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
            ShadowRootParams[1].Descriptor.ShaderRegister = 1;
            ShadowRootParams[1].Descriptor.RegisterSpace = 0;
            ShadowRootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

            // 2: Descriptor table for albedo texture (t0) - for alpha testing
            ShadowRootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            ShadowRootParams[2].DescriptorTable.NumDescriptorRanges = 1;
            ShadowRootParams[2].DescriptorTable.pDescriptorRanges = &AlbedoSRVRange;
            ShadowRootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

            // Static sampler for texture sampling
            D3D12_STATIC_SAMPLER_DESC ShadowSampler = {};
            ShadowSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            ShadowSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            ShadowSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            ShadowSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            ShadowSampler.ShaderRegister = 0; // s0
            ShadowSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

            D3D12_ROOT_SIGNATURE_DESC ShadowRootSigDesc = {};
            ShadowRootSigDesc.NumParameters = 3;
            ShadowRootSigDesc.pParameters = ShadowRootParams;
            ShadowRootSigDesc.NumStaticSamplers = 1;
            ShadowRootSigDesc.pStaticSamplers = &ShadowSampler;
            ShadowRootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

            ComPtr<ID3DBlob> ShadowSigBlob;
            Hr = D3D12SerializeRootSignature(&ShadowRootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                &ShadowSigBlob, &m_ErrorBlob);
            LogD3DCompileFailure("Failed to serialize shadow root signature.");

            Hr = GetDeviceContext().m_Device->CreateRootSignature(
                0,
                ShadowSigBlob->GetBufferPointer(),
                ShadowSigBlob->GetBufferSize(),
                HOX::Win32::UuidOf<ID3D12RootSignature>(),
                HOX::Win32::PpvArgs(m_ShadowRootSignature.ReleaseAndGetAddressOf()));
            LogD3DCompileFailure("Failed to create shadow root signature.");
        }

        // SSAO Root Signature
        {
            D3D12_DESCRIPTOR_RANGE DepthSRVRange = {};
            DepthSRVRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            DepthSRVRange.NumDescriptors = 1;
            DepthSRVRange.BaseShaderRegister = 0; // t0
            DepthSRVRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

            D3D12_DESCRIPTOR_RANGE NoiseSRVRange = {};
            NoiseSRVRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            NoiseSRVRange.NumDescriptors = 1;
            NoiseSRVRange.BaseShaderRegister = 1; // t1
            NoiseSRVRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

            D3D12_DESCRIPTOR_RANGE OutputUAVRange = {};
            OutputUAVRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
            OutputUAVRange.NumDescriptors = 1;
            OutputUAVRange.BaseShaderRegister = 0; // u0
            OutputUAVRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

            D3D12_ROOT_PARAMETER SSAORootParams[4] = {};

            // 0: CBV for SSAO constants (b0)
            SSAORootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
            SSAORootParams[0].Descriptor.ShaderRegister = 0;
            SSAORootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

            // 1: Depth SRV (t0)
            SSAORootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            SSAORootParams[1].DescriptorTable.NumDescriptorRanges = 1;
            SSAORootParams[1].DescriptorTable.pDescriptorRanges = &DepthSRVRange;
            SSAORootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

            // 2: Noise SRV (t1)
            SSAORootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            SSAORootParams[2].DescriptorTable.NumDescriptorRanges = 1;
            SSAORootParams[2].DescriptorTable.pDescriptorRanges = &NoiseSRVRange;
            SSAORootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

            // 3: Output UAV (u0)
            SSAORootParams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            SSAORootParams[3].DescriptorTable.NumDescriptorRanges = 1;
            SSAORootParams[3].DescriptorTable.pDescriptorRanges = &OutputUAVRange;
            SSAORootParams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

            // Static sampler for noise texture
            D3D12_STATIC_SAMPLER_DESC NoiseSampler = {};
            NoiseSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            NoiseSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            NoiseSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            NoiseSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            NoiseSampler.ShaderRegister = 0;
            NoiseSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

            D3D12_ROOT_SIGNATURE_DESC SSAORootSigDesc = {};
            SSAORootSigDesc.NumParameters = 4;
            SSAORootSigDesc.pParameters = SSAORootParams;
            SSAORootSigDesc.NumStaticSamplers = 1;
            SSAORootSigDesc.pStaticSamplers = &NoiseSampler;

            ComPtr<ID3DBlob> SSAOSigBlob;
            Hr = D3D12SerializeRootSignature(&SSAORootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                &SSAOSigBlob, &m_ErrorBlob);
            LogD3DCompileFailure("Failed to serialize SSAO root signature.");

            Hr = GetDeviceContext().m_Device->CreateRootSignature(
                0,
                SSAOSigBlob->GetBufferPointer(),
                SSAOSigBlob->GetBufferSize(),
                HOX::Win32::UuidOf<ID3D12RootSignature>(),
                HOX::Win32::PpvArgs(m_SSAORootSignature.ReleaseAndGetAddressOf()));
            LogD3DCompileFailure("Failed to create SSAO root signature.");
        }

        // SSAO Blur Root Signature
        {
            D3D12_DESCRIPTOR_RANGE InputSRVRange = {};
            InputSRVRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
            InputSRVRange.NumDescriptors = 1;
            InputSRVRange.BaseShaderRegister = 0; // t0
            InputSRVRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

            D3D12_DESCRIPTOR_RANGE OutputUAVRange = {};
            OutputUAVRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
            OutputUAVRange.NumDescriptors = 1;
            OutputUAVRange.BaseShaderRegister = 0; // u0
            OutputUAVRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

            D3D12_ROOT_PARAMETER BlurRootParams[3] = {};

            // 0: Blur constants (root constants)
            BlurRootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
            BlurRootParams[0].Constants.ShaderRegister = 0;
            BlurRootParams[0].Constants.Num32BitValues = 4; // width, height, padding x2
            BlurRootParams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

            // 1: Input SRV (t0)
            BlurRootParams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            BlurRootParams[1].DescriptorTable.NumDescriptorRanges = 1;
            BlurRootParams[1].DescriptorTable.pDescriptorRanges = &InputSRVRange;
            BlurRootParams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

            // 2: Output UAV (u0)
            BlurRootParams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
            BlurRootParams[2].DescriptorTable.NumDescriptorRanges = 1;
            BlurRootParams[2].DescriptorTable.pDescriptorRanges = &OutputUAVRange;
            BlurRootParams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

            D3D12_ROOT_SIGNATURE_DESC BlurRootSigDesc = {};
            BlurRootSigDesc.NumParameters = 3;
            BlurRootSigDesc.pParameters = BlurRootParams;

            ComPtr<ID3DBlob> BlurSigBlob;
            Hr = D3D12SerializeRootSignature(&BlurRootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                &BlurSigBlob, &m_ErrorBlob);
            LogD3DCompileFailure("Failed to serialize SSAO blur root signature.");

            Hr = GetDeviceContext().m_Device->CreateRootSignature(
                0,
                BlurSigBlob->GetBufferPointer(),
                BlurSigBlob->GetBufferSize(),
                HOX::Win32::UuidOf<ID3D12RootSignature>(),
                HOX::Win32::PpvArgs(m_SSAOBlurRootSignature.ReleaseAndGetAddressOf()));
            LogD3DCompileFailure("Failed to create SSAO blur root signature.");
        }

        // SSAO Compute PSO
        {
            D3D12_COMPUTE_PIPELINE_STATE_DESC SSAOPSODesc = {};
            SSAOPSODesc.pRootSignature = m_SSAORootSignature.Get();
            SSAOPSODesc.CS = {
                m_SSAOCSBlob->GetBufferPointer(),
                m_SSAOCSBlob->GetBufferSize()
            };

            Hr = GetDeviceContext().m_Device->CreateComputePipelineState(
                &SSAOPSODesc,
                HOX::Win32::UuidOf<ID3D12PipelineState>(),
                HOX::Win32::PpvArgs(m_SSAOPSO.ReleaseAndGetAddressOf()));
            LogD3DCompileFailure("Failed to create SSAO compute PSO.");
        }

        // SSAO Blur Compute PSO
        {
            D3D12_COMPUTE_PIPELINE_STATE_DESC BlurPSODesc = {};
            BlurPSODesc.pRootSignature = m_SSAOBlurRootSignature.Get();
            BlurPSODesc.CS = {
                m_SSAOBlurCSBlob->GetBufferPointer(),
                m_SSAOBlurCSBlob->GetBufferSize()
            };

            Hr = GetDeviceContext().m_Device->CreateComputePipelineState(
                &BlurPSODesc,
                HOX::Win32::UuidOf<ID3D12PipelineState>(),
                HOX::Win32::PpvArgs(m_SSAOBlurPSO.ReleaseAndGetAddressOf()));
            LogD3DCompileFailure("Failed to create SSAO blur compute PSO.");
        }

        {
            D3D12_HEAP_PROPERTIES HeapProps = {};
            HeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

            D3D12_RESOURCE_DESC ResourceDesc = {};
            ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            ResourceDesc.Width = sizeof(CullingConstants);
            ResourceDesc.Height = 1;
            ResourceDesc.DepthOrArraySize = 1;
            ResourceDesc.MipLevels = 1;
            ResourceDesc.SampleDesc.Count = 1;
            ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

            GetDeviceContext().m_Device->CreateCommittedResource(
                &HeapProps,
                D3D12_HEAP_FLAG_NONE,
                &ResourceDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                HOX::Win32::UuidOf<ID3D12Resource>(),
                HOX::Win32::PpvArgs(m_CullingConstantsBuffer.ReleaseAndGetAddressOf()));

            D3D12_RANGE ReadRange = {0, 0};
            m_CullingConstantsBuffer->Map(0, &ReadRange, &m_CullingConstantsMapped);
        }

        // Tone Mapping Constants Buffer
        {
            D3D12_HEAP_PROPERTIES HeapProps = {};
            HeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

            D3D12_RESOURCE_DESC ResourceDesc = {};
            ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            ResourceDesc.Width = 256; // Minimum CB alignment
            ResourceDesc.Height = 1;
            ResourceDesc.DepthOrArraySize = 1;
            ResourceDesc.MipLevels = 1;
            ResourceDesc.SampleDesc.Count = 1;
            ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

            GetDeviceContext().m_Device->CreateCommittedResource(
                &HeapProps,
                D3D12_HEAP_FLAG_NONE,
                &ResourceDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                nullptr,
                HOX::Win32::UuidOf<ID3D12Resource>(),
                HOX::Win32::PpvArgs(m_ToneMappingConstantsBuffer.ReleaseAndGetAddressOf()));

            D3D12_RANGE ReadRange = {0, 0};
            m_ToneMappingConstantsBuffer->Map(0, &ReadRange, &m_ToneMappingConstantsMapped);

            // Initialize with default exposure
            ToneMappingConstants toneConsts{};
            toneConsts.Exposure = m_Exposure;
            memcpy(m_ToneMappingConstantsMapped, &toneConsts, sizeof(ToneMappingConstants));
        }

        m_LightManager = std::make_unique<LightManager>();
        m_LightManager->Initialize(m_SRVHeap.get(),10000000);

        {
            GPULight Light{};

            // Directional light (sun) - illuminates entire scene
            Light.m_Type = LightType::Directional;
            DirectionValue += 0.001f;
            Light.m_Direction = {-.5, -.01, -.0f};
            Light.m_Color = {1.0f, 0.95f, 0.8f}; // Warm sunlight
            Light.m_Intensity = 3.f;
            Light.Range = 0.0f;
            DirLightIdx = m_LightManager->AddLight(Light);

            Light.m_Type = LightType::Point;
            Light.Range = 400.0f;
            Light.m_Intensity = 20.0f;

            const int GRID_X = 30;
            const int GRID_Y = 30;
            const int GRID_Z = 30;
            const float SPACING = 500.0f;
            const float BASE_HEIGHT = -3000.0f;

            int index = 0;
            const int TOTAL_LIGHTS = GRID_X * GRID_Y * GRID_Z;

            for (int x = 0; x < GRID_X; x++)
            {
                for (int y = 0; y < GRID_Y; y++)
                {
                    for (int z = 0; z < GRID_Z; z++)
                    {
                        // Position (3D cube)
                        Light.m_Position = {
                            (x - GRID_X / 2) * SPACING,
                            BASE_HEIGHT + y * SPACING,
                            (z - GRID_Z / 2) * SPACING
                        };

                        // Unique color using HSV → RGB
                        float h = float(index) / float(TOTAL_LIGHTS);
                        float s = 1.0f;
                        float v = 1.0f;

                        float r, g, b;
                        float i = floor(h * 6.0f);
                        float f = h * 6.0f - i;
                        float p = v * (1.0f - s);
                        float q = v * (1.0f - f * s);
                        float t = v * (1.0f - (1.0f - f) * s);

                        switch (int(i) % 6) {
                            case 0: r = v; g = t; b = p; break;
                            case 1: r = q; g = v; b = p; break;
                            case 2: r = p; g = v; b = t; break;
                            case 3: r = p; g = q; b = v; break;
                            case 4: r = t; g = p; b = v; break;
                            case 5: r = v; g = p; b = q; break;
                        }

                        Light.m_Color = { r, g, b };

                        m_LightManager->AddLight(Light);
                        index++;
                    }
                }
            }

            m_LightManager->UpdateGPUBuffer();
        }

        m_TileCullingBuffers = std::make_unique<TileCullingBuffers>();
        m_TileCullingBuffers->Initialize(
            GetDeviceContext().m_WindowWidth,
            GetDeviceContext().m_WindowHeight,
            m_SRVHeap.get());

        // Initialize Cascaded Shadow Maps
        m_CascadedShadowMap = std::make_unique<CascadedShadowMap>();
        m_CascadedShadowMap->Initialize(m_SRVHeap.get());

        // Initialize SSAO
        m_SSAO = std::make_unique<SSAO>();
        m_SSAO->Initialize(
            GetDeviceContext().m_WindowWidth,
            GetDeviceContext().m_WindowHeight,
            m_SRVHeap.get());

        D3D12_GRAPHICS_PIPELINE_STATE_DESC GraphicsPipelineDesc = {};
        GraphicsPipelineDesc.pRootSignature = m_RootSignature.Get();
        GraphicsPipelineDesc.VS = {
            m_VertexShaderBlob->GetBufferPointer(),
            m_VertexShaderBlob->GetBufferSize(),
        };
        GraphicsPipelineDesc.PS = {
            m_PixelShaderBlob->GetBufferPointer(),
            m_PixelShaderBlob->GetBufferSize(),
        };

        // Connecting c++ vertex struct to the shader input

        // MeshVertex layout: Position(12) + Normal(12) + Tangent(12) + TexCoord(8) + Color(16) = 60 bytes
        D3D12_INPUT_ELEMENT_DESC InputLayoutDesc[] = {
            {
                "POSITION",
                0,
                DXGI_FORMAT_R32G32B32_FLOAT, // float3
                0,
                0, // offset 0
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                0
            },
            {
                "NORMAL",
                0,
                DXGI_FORMAT_R32G32B32_FLOAT, // float3
                0,
                12, // offset 12 (after position)
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                0
            },
            {
                "TANGENT",
                0,
                DXGI_FORMAT_R32G32B32_FLOAT, // float3
                0,
                24, // offset 24 (after position + normal)
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                0
            },
            {
                "TEXCOORD",
                0,
                DXGI_FORMAT_R32G32_FLOAT, // float2
                0,
                36, // offset 36 (after position + normal + tangent)
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                0
            },
            {
                "COLOR",
                0,
                DXGI_FORMAT_R32G32B32A32_FLOAT, // float4
                0,
                44, // offset 44 (after position + normal + tangent + texcoord)
                D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
                0
            }
        };

        m_ModelLoader = std::make_unique<HOX::ModelLoader>();
        m_GO = std::make_unique<HOX::GameObject>();
        m_Scene = std::make_unique<Scene>();

        m_CommandAllocators[0]->Reset();
        m_CommandList->Reset(m_CommandAllocators[0].Get(), nullptr);

        m_GO->m_Model = std::move(
            m_ModelLoader->LoadFromFile("../Resources/Cyberpunk/scene.gltf", m_CommandList.Get(), m_SRVHeap.get()));

        GetDeviceContext().m_CommandSystem->ExecuteAndFlush(
            m_CommandList.Get(),
            m_CommandAllocators[0].Get(),
            m_Fence->GetFence().Get(),
            m_Fence->GetFenceValue(),
            m_Fence->GetFenceEvent());
        m_CommandList->Close();

        u64 currentFenceValue = m_Fence->GetFenceValue();
        for (u32 i = 0; i < MaxFrames; ++i) {
            m_SwapChain->m_FrameFenceValues[i] = currentFenceValue;
        }

        m_GO->m_Transform.Position = {0.f, 0.f, 0.f};
        //m_GO->m_Transform.SetRotationEuler(-90.f,90.f,0.f);
        m_GO->CreateConstantBuffer();
        m_Scene->AddGameObject(std::move(m_GO));

        Logger::LogMessage(Severity::Info, "Scene system initialized");


        const int WindowWidth = static_cast<int>(GetDeviceContext().m_WindowWidth);
        const int WindowHeight = static_cast<int>(GetDeviceContext().m_WindowHeight);
        CreateDepthBuffer(WindowWidth, WindowHeight);


        GraphicsPipelineDesc.InputLayout = {InputLayoutDesc, _countof(InputLayoutDesc)};
        GraphicsPipelineDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        GraphicsPipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        // Fillmode = Solid not wireframe; CullMode = Back; ClockWise = Front;

        GraphicsPipelineDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        // No blending just overwrite pixels, will need to change for transparency

        auto& rt0 = GraphicsPipelineDesc.BlendState.RenderTarget[0];
        rt0.BlendEnable           = TRUE;
        rt0.LogicOpEnable         = FALSE;
        rt0.SrcBlend              = D3D12_BLEND_SRC_ALPHA;
        rt0.DestBlend             = D3D12_BLEND_INV_SRC_ALPHA;
        rt0.BlendOp               = D3D12_BLEND_OP_ADD;
        rt0.SrcBlendAlpha         = D3D12_BLEND_ONE;
        rt0.DestBlendAlpha        = D3D12_BLEND_INV_SRC_ALPHA;   // or ONE if you don’t care about alpha in RT
        rt0.BlendOpAlpha          = D3D12_BLEND_OP_ADD;
        rt0.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

        // Disable depth , no depth buffer yet
        GraphicsPipelineDesc.DepthStencilState.DepthEnable = true;
        GraphicsPipelineDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        GraphicsPipelineDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
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
        LogD3DCompileFailure("Failed to create graphics pipeline "); {
            D3D12_GRAPHICS_PIPELINE_STATE_DESC DepthPSODesc = {GraphicsPipelineDesc};

            DepthPSODesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
            DepthPSODesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;

            DepthPSODesc.VS =
            {
                m_DepthVertexShaderShaderBlob->GetBufferPointer(),
                m_DepthVertexShaderShaderBlob->GetBufferSize(),
            };

            // Pixel shader for alpha testing (transparent objects like leaves)
            DepthPSODesc.PS = {
                m_DepthPixelShaderBlob->GetBufferPointer(),
                m_DepthPixelShaderBlob->GetBufferSize()
            };

            DepthPSODesc.NumRenderTargets = 0;
            DepthPSODesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;

            Hr = GetDeviceContext().m_Device->CreateGraphicsPipelineState(
                &DepthPSODesc,
                HOX::Win32::UuidOf<ID3D12PipelineState>(),
                HOX::Win32::PpvArgs(m_DepthPSO.ReleaseAndGetAddressOf()));

            LogD3DCompileFailure("Failed to create depth ");
        }

        // Shadow Map PSO (depth-only rendering from light's perspective)
        {
            D3D12_GRAPHICS_PIPELINE_STATE_DESC ShadowPSODesc = {};
            ShadowPSODesc.pRootSignature = m_ShadowRootSignature.Get();
            ShadowPSODesc.VS = {
                m_ShadowVSBlob->GetBufferPointer(),
                m_ShadowVSBlob->GetBufferSize()
            };
            // Pixel shader for alpha testing (transparent objects like leaves)
            ShadowPSODesc.PS = {
                m_ShadowPSBlob->GetBufferPointer(),
                m_ShadowPSBlob->GetBufferSize()
            };

            // Use same input layout as main pass
            D3D12_INPUT_ELEMENT_DESC ShadowInputLayout[] = {
                {
                    "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,
                    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
                },
                {
                    "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12,
                    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
                },
                {
                    "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 24,
                    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
                },
                {
                    "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 36,
                    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
                },
                {
                    "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 44,
                    D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
                }
            };

            ShadowPSODesc.InputLayout = {ShadowInputLayout, _countof(ShadowInputLayout)};
            ShadowPSODesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
            ShadowPSODesc.RasterizerState.DepthBias = 1000;
            ShadowPSODesc.RasterizerState.DepthBiasClamp = 0.0f;
            ShadowPSODesc.RasterizerState.SlopeScaledDepthBias = 1.5f;
            ShadowPSODesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
            ShadowPSODesc.DepthStencilState.DepthEnable = TRUE;
            ShadowPSODesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
            ShadowPSODesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
            ShadowPSODesc.DepthStencilState.StencilEnable = FALSE;
            ShadowPSODesc.SampleMask = UINT_MAX;
            ShadowPSODesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            ShadowPSODesc.NumRenderTargets = 0;
            ShadowPSODesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
            ShadowPSODesc.SampleDesc.Count = 1;
            ShadowPSODesc.SampleDesc.Quality = 0;

            Hr = GetDeviceContext().m_Device->CreateGraphicsPipelineState(
                &ShadowPSODesc,
                HOX::Win32::UuidOf<ID3D12PipelineState>(),
                HOX::Win32::PpvArgs(m_ShadowPSO.ReleaseAndGetAddressOf()));
            LogD3DCompileFailure("Failed to create shadow PSO.");
        }

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

        // Camera movement && and binding
        {
            const DirectX::XMMATRIX ViewProjection = m_Camera->GetViewProjectionMatrix();

            CameraConstants Constants{};
            DirectX::XMStoreFloat4x4(&Constants.m_ViewProjection, ViewProjection);
            Constants.m_CameraPosition = m_Camera->GetPosition();
            memcpy(m_CameraConstantBufferMapped, &Constants, sizeof(Constants));
        }

        // ===========================================
        // CSM Shadow Pass - Render scene from light's perspective for each cascade
        // ===========================================
        {
            // Get directional light for shadow casting
            const GPULight* DirLight = m_LightManager->GetDirectionalLight();
            if (DirLight && m_CascadedShadowMap && m_Scene) {
                // Update CSM matrices based on camera frustum
                DirectX::XMFLOAT3 LightDir = DirLight->m_Direction;
                m_CascadedShadowMap->Update(
                    m_Camera->GetViewMatrix(),
                    m_Camera->GetProjectionMatrix(),
                    LightDir,
                    m_Camera->GetNearPlane(),
                    m_Camera->GetFarPlane()
                );

                // Store light direction in shadow constants
                m_CascadedShadowMap->SetLightDirection(LightDir);
                m_CascadedShadowMap->UpdateConstantBuffer(m_Camera->GetViewMatrix());

                // Set shadow PSO and root signature
                m_CommandList->SetGraphicsRootSignature(m_ShadowRootSignature.Get());
                m_CommandList->SetPipelineState(m_ShadowPSO.Get());
                m_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

                // Set descriptor heap for texture access (alpha testing)
                ID3D12DescriptorHeap* Heaps[] = { m_SRVHeap->GetD3D12DescriptorHeap() };
                m_CommandList->SetDescriptorHeaps(1, Heaps);

                // Shadow map viewport and scissor
                D3D12_VIEWPORT ShadowViewport = {};
                ShadowViewport.Width = static_cast<float>(CSM_SHADOW_MAP_SIZE);
                ShadowViewport.Height = static_cast<float>(CSM_SHADOW_MAP_SIZE);
                ShadowViewport.MinDepth = 0.0f;
                ShadowViewport.MaxDepth = 1.0f;

                D3D12_RECT ShadowScissor = {};
                ShadowScissor.right = CSM_SHADOW_MAP_SIZE;
                ShadowScissor.bottom = CSM_SHADOW_MAP_SIZE;

                m_CommandList->RSSetViewports(1, &ShadowViewport);
                m_CommandList->RSSetScissorRects(1, &ShadowScissor);

                // Render each cascade
                for (u32 cascade = 0; cascade < CSM_NUM_CASCADES; cascade++) {
                    // Get DSV for this cascade
                    D3D12_CPU_DESCRIPTOR_HANDLE CascadeDSV = m_CascadedShadowMap->GetDSVHandle(cascade);

                    // Set render target (no RTV, depth only)
                    m_CommandList->OMSetRenderTargets(0, nullptr, FALSE, &CascadeDSV);
                    m_CommandList->ClearDepthStencilView(CascadeDSV, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

                    // Bind light view-projection for this cascade (b0)
                    // We need a per-cascade constant buffer, using the main CB and updating per cascade
                    // For simplicity, we'll create a temporary buffer approach
                    // Actually, let's use the cascade VP directly

                    // Create a small upload buffer for the cascade VP (or reuse mapped buffer)
                    // For now, let's bind the cascade VP from the shadow constants
                    // The shadow shader expects b0 = LightViewProjection matrix

                    // We'll use a simple approach: copy the cascade matrix to a temp location
                    // Since we have the shadow constant buffer, we can offset into it

                    // Better approach: bind the full shadow constants and use cascade index
                    // But the shader expects just a single matrix at b0

                    // Let's modify to use a per-frame cascade buffer or root constants
                    // For efficiency, use root constants for the 64-byte matrix

                    const DirectX::XMFLOAT4X4& CascadeVP = m_CascadedShadowMap->GetCascadeViewProjection(cascade);
                    m_CommandList->SetGraphicsRoot32BitConstants(0, 16, &CascadeVP, 0);

                    // Draw scene for this cascade with alpha testing
                    m_Scene->DrawShadow(m_CommandList.Get(), m_SRVHeap.get(), m_DefaultTexture->GetSRVIndex());
                }

                // Transition shadow maps from depth write to pixel shader resource
                CD3DX12_RESOURCE_BARRIER ShadowBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
                    m_CascadedShadowMap->GetResource(),
                    D3D12_RESOURCE_STATE_DEPTH_WRITE,
                    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
                );
                m_CommandList->ResourceBarrier(1, &ShadowBarrier);
            }
        }

        // Depth pass (with alpha testing for transparent objects)
        {
            D3D12_CPU_DESCRIPTOR_HANDLE DSV = m_DSVHeap->GetCPUDescriptorHandleForHeapStart();
            m_CommandList->OMSetRenderTargets(0, nullptr, FALSE, &DSV);
            m_CommandList->ClearDepthStencilView(DSV, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

            m_CommandList->SetGraphicsRootSignature(m_RootSignature.Get());
            m_CommandList->SetPipelineState(m_DepthPSO.Get());
            m_CommandList->RSSetViewports(1, &m_Viewport);
            m_CommandList->RSSetScissorRects(1, &m_ScissorRect);
            m_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            // Set descriptor heap for texture access (alpha testing)
            ID3D12DescriptorHeap* DepthHeaps[] = { m_SRVHeap->GetD3D12DescriptorHeap() };
            m_CommandList->SetDescriptorHeaps(1, DepthHeaps);

            m_CommandList->SetGraphicsRootConstantBufferView(RootParams::CameraCBV,
                                                             m_CameraConstantbuffer->GetGPUVirtualAddress());

            if (m_Scene) {
                m_Scene->DrawDepthOnly(m_CommandList.Get(), m_SRVHeap.get(), m_DefaultTexture->GetSRVIndex());
            }
        }

        // ===========================================
        // SSAO Compute Pass - Generate ambient occlusion from depth buffer
        // ===========================================
        {
            // Transition depth buffer from depth write to shader resource
            CD3DX12_RESOURCE_BARRIER DepthBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
                m_DepthStencilBuffer.Get(),
                D3D12_RESOURCE_STATE_DEPTH_WRITE,
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
            );
            m_CommandList->ResourceBarrier(1, &DepthBarrier);

            // Update SSAO constants
            DirectX::XMMATRIX InvProj = DirectX::XMMatrixInverse(nullptr, m_Camera->GetProjectionMatrix());
            m_SSAO->UpdateConstants(
                m_Camera->GetProjectionMatrix(),
                InvProj,
                static_cast<u32>(m_Viewport.Width),
                static_cast<u32>(m_Viewport.Height)
            );

            // Set descriptor heap
            ID3D12DescriptorHeap* Heaps[] = { m_SRVHeap->GetD3D12DescriptorHeap() };
            m_CommandList->SetDescriptorHeaps(1, Heaps);

            // === SSAO Main Pass ===
            m_CommandList->SetComputeRootSignature(m_SSAORootSignature.Get());
            m_CommandList->SetPipelineState(m_SSAOPSO.Get());

            // Bind resources
            // 0: CBV for SSAO constants (b0)
            m_CommandList->SetComputeRootConstantBufferView(0, m_SSAO->GetConstantBuffer()->GetGPUVirtualAddress());
            // 1: Depth SRV (t0)
            m_CommandList->SetComputeRootDescriptorTable(1, m_SRVHeap->GetGPUHandle(m_DepthBufferSRVIndex));
            // 2: Noise SRV (t1)
            m_CommandList->SetComputeRootDescriptorTable(2, m_SRVHeap->GetGPUHandle(m_SSAO->GetNoiseSRVIndex()));
            // 3: Output UAV (u0)
            m_CommandList->SetComputeRootDescriptorTable(3, m_SRVHeap->GetGPUHandle(m_SSAO->GetSSAOOutputUAVIndex()));

            // Dispatch SSAO compute shader (8x8 thread groups)
            u32 dispatchX = (static_cast<u32>(m_Viewport.Width) + 7) / 8;
            u32 dispatchY = (static_cast<u32>(m_Viewport.Height) + 7) / 8;
            m_CommandList->Dispatch(dispatchX, dispatchY, 1);

            // UAV barrier to ensure SSAO write completes before blur reads
            D3D12_RESOURCE_BARRIER UAVBarrier = {};
            UAVBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            UAVBarrier.UAV.pResource = m_SSAO->GetSSAOOutput();
            m_CommandList->ResourceBarrier(1, &UAVBarrier);

            // Transition SSAO output from UAV to SRV for blur pass
            CD3DX12_RESOURCE_BARRIER SSAOBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
                m_SSAO->GetSSAOOutput(),
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
            );
            m_CommandList->ResourceBarrier(1, &SSAOBarrier);

            // === SSAO Blur Pass ===
            m_CommandList->SetComputeRootSignature(m_SSAOBlurRootSignature.Get());
            m_CommandList->SetPipelineState(m_SSAOBlurPSO.Get());

            // Bind blur constants (root constants: width, height, padding x2)
            u32 blurConstants[4] = {
                static_cast<u32>(m_Viewport.Width),
                static_cast<u32>(m_Viewport.Height),
                0, 0
            };
            m_CommandList->SetComputeRoot32BitConstants(0, 4, blurConstants, 0);
            // 1: Input SRV (t0) - raw SSAO
            m_CommandList->SetComputeRootDescriptorTable(1, m_SRVHeap->GetGPUHandle(m_SSAO->GetSSAOOutputSRVIndex()));
            // 2: Output UAV (u0) - blurred SSAO
            m_CommandList->SetComputeRootDescriptorTable(2, m_SRVHeap->GetGPUHandle(m_SSAO->GetSSAOBlurredUAVIndex()));

            // Dispatch blur
            m_CommandList->Dispatch(dispatchX, dispatchY, 1);

            // UAV barrier for blur output
            UAVBarrier.UAV.pResource = m_SSAO->GetSSAOBlurred();
            m_CommandList->ResourceBarrier(1, &UAVBarrier);

            // Transition blurred SSAO to pixel shader resource for color pass
            CD3DX12_RESOURCE_BARRIER BlurredBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
                m_SSAO->GetSSAOBlurred(),
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
            );
            m_CommandList->ResourceBarrier(1, &BlurredBarrier);

            // Transition raw SSAO back to UAV for next frame
            SSAOBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
                m_SSAO->GetSSAOOutput(),
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS
            );
            m_CommandList->ResourceBarrier(1, &SSAOBarrier);

            // Keep depth buffer as SRV for light culling (it will transition back after light culling)
        }

        // Light Culling Compute Pass
        {
            // Depth buffer is already in SRV state from SSAO pass
            // Transition to include PIXEL_SHADER_RESOURCE for later use
            CD3DX12_RESOURCE_BARRIER DepthBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
                m_DepthStencilBuffer.Get(),
                D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE
            );
            m_CommandList->ResourceBarrier(1, &DepthBarrier);

            // Set compute root signature and PSO
            m_CommandList->SetComputeRootSignature(m_ComputeRootSignature.Get());
            m_CommandList->SetPipelineState(m_LightCullingPipelineState.Get());

            // Update culling constants
            CullingConstants CullConsts{};
            DirectX::XMStoreFloat4x4(&CullConsts.View, m_Camera->GetViewMatrix());
            DirectX::XMStoreFloat4x4(&CullConsts.Projection, m_Camera->GetProjectionMatrix());

            DirectX::XMMATRIX InvProj = DirectX::XMMatrixInverse(nullptr, m_Camera->GetProjectionMatrix());
            DirectX::XMStoreFloat4x4(&CullConsts.InverseProjection, InvProj);

            CullConsts.ScreenWidth = static_cast<u32>(m_Viewport.Width);
            CullConsts.ScreenHeight = static_cast<u32>(m_Viewport.Height);
            CullConsts.TileCountX = m_TileCullingBuffers->GetXTileCount();
            CullConsts.TileCountY = m_TileCullingBuffers->GetYTileCount();
            CullConsts.LightCount = m_LightManager->GetLightCount();

            memcpy(m_CullingConstantsMapped, &CullConsts, sizeof(CullConsts));

            // Set descriptor heap
            ID3D12DescriptorHeap* Heaps[] = { m_SRVHeap->GetD3D12DescriptorHeap() };
            m_CommandList->SetDescriptorHeaps(1, Heaps);

            // Bind root parameters
            // 0: b0 - culling constants
            m_CommandList->SetComputeRootConstantBufferView(0, m_CullingConstantsBuffer->GetGPUVirtualAddress());

            // 1: t0 - lights SRV
            m_CommandList->SetComputeRootDescriptorTable(1, m_SRVHeap->GetGPUHandle(m_LightManager->GetSRVIndex()));

            // 2: t1 - depth SRV
            m_CommandList->SetComputeRootDescriptorTable(2, m_SRVHeap->GetGPUHandle(m_DepthBufferSRVIndex));

            // 3: u0 - light grid UAV
            m_CommandList->SetComputeRootDescriptorTable(3, m_SRVHeap->GetGPUHandle(m_TileCullingBuffers->GetLightGridUAVIndex()));

            // 4: u1 - light index list UAV
            m_CommandList->SetComputeRootDescriptorTable(4, m_SRVHeap->GetGPUHandle(m_TileCullingBuffers->GetLightIndexListUAVIndex()));

            // 5: u2 - counter UAV
            m_CommandList->SetComputeRootDescriptorTable(5, m_SRVHeap->GetGPUHandle(m_TileCullingBuffers->GetCounterUAVIndex()));

            // Clear the counter to zero before dispatch using copy from upload buffer
            // Transition counter buffer to copy dest
            CD3DX12_RESOURCE_BARRIER CounterBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
                m_TileCullingBuffers->GetCounterResource(),
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                D3D12_RESOURCE_STATE_COPY_DEST);
            m_CommandList->ResourceBarrier(1, &CounterBarrier);

            m_CommandList->CopyBufferRegion(
                m_TileCullingBuffers->GetCounterResource(), 0,
                m_TileCullingBuffers->GetCounterZeroBuffer(), 0,
                sizeof(u32));

            // Transition counter buffer back to UAV
            CounterBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
                m_TileCullingBuffers->GetCounterResource(),
                D3D12_RESOURCE_STATE_COPY_DEST,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            m_CommandList->ResourceBarrier(1, &CounterBarrier);

            // Dispatch: one thread group per tile
            m_CommandList->Dispatch(
                m_TileCullingBuffers->GetXTileCount(),
                m_TileCullingBuffers->GetYTileCount(),
                1);

            // UAV barrier to ensure compute shader has finished writing before pixel shader reads
            D3D12_RESOURCE_BARRIER UAVBarrier = {};
            UAVBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            UAVBarrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            UAVBarrier.UAV.pResource = nullptr; // Global UAV barrier
            m_CommandList->ResourceBarrier(1, &UAVBarrier);

            // Transition depth buffer back to depth write
            DepthBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
                m_DepthStencilBuffer.Get(),
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_DEPTH_WRITE
            );
            m_CommandList->ResourceBarrier(1, &DepthBarrier);

            // Transition UAVs to SRV for pixel shader read
            D3D12_RESOURCE_BARRIER UAVBarriers[2] = {};
            UAVBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
                m_TileCullingBuffers->GetLightGridResource(),
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            UAVBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
                m_TileCullingBuffers->GetLightIndexListResource(),
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
            m_CommandList->ResourceBarrier(2, UAVBarriers);
        }

        // Color Draw
        {

            CD3DX12_CPU_DESCRIPTOR_HANDLE RTV(
                m_RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
                CurrentBackBufferIndex,
                m_RTVDescriptorSize);
            D3D12_CPU_DESCRIPTOR_HANDLE DSV = m_DSVHeap->GetCPUDescriptorHandleForHeapStart();

            m_CommandList->OMSetRenderTargets(1, &RTV, FALSE, &DSV);
            const FLOAT clearColor[4] = {0.4f, 0.4f, 0.4f, 1.0f};
            m_CommandList->ClearRenderTargetView(RTV, clearColor, 0, nullptr);

            m_CommandList->SetGraphicsRootSignature(m_RootSignature.Get());
            m_CommandList->SetPipelineState(m_PipelineState.Get());

            ID3D12DescriptorHeap *Heaps[] = {m_SRVHeap->GetD3D12DescriptorHeap()};
            m_CommandList->SetDescriptorHeaps(1, Heaps);

            // Bind camera constants (b0) - already bound from depth pass but need to re-bind for graphics
            m_CommandList->SetGraphicsRootConstantBufferView(RootParams::CameraCBV,
                                                              m_CameraConstantbuffer->GetGPUVirtualAddress());

            // Bind default textures (will be overridden per-mesh in Scene::Render)
            m_CommandList->SetGraphicsRootDescriptorTable(RootParams::TextureSRV,
                                                          m_SRVHeap->GetGPUHandle(m_DefaultTexture->GetSRVIndex()));
            m_CommandList->SetGraphicsRootDescriptorTable(RootParams::NormalMapSRV,
                                                          m_SRVHeap->GetGPUHandle(m_DefaultNormalMap->GetSRVIndex()));
            m_CommandList->SetGraphicsRootDescriptorTable(RootParams::MetallicRoughnessSRV,
                                                          m_SRVHeap->GetGPUHandle(m_DefaultMetallicRoughness->GetSRVIndex()));

            // Bind light data SRVs
            m_CommandList->SetGraphicsRootDescriptorTable(RootParams::LightsSRV,
                                                          m_SRVHeap->GetGPUHandle(m_LightManager->GetSRVIndex()));
            m_CommandList->SetGraphicsRootDescriptorTable(RootParams::LightGridSRV,
                                                          m_SRVHeap->GetGPUHandle(m_TileCullingBuffers->GetLightGridSRVIndex()));
            m_CommandList->SetGraphicsRootDescriptorTable(RootParams::LightIndexListSRV,
                                                          m_SRVHeap->GetGPUHandle(m_TileCullingBuffers->GetLightIndexListSRVIndex()));

            // Set screen constants as root constants
            ScreenConstants screenConsts{};
            screenConsts.ScreenWidth = static_cast<u32>(m_Viewport.Width);
            screenConsts.ScreenHeight = static_cast<u32>(m_Viewport.Height);
            screenConsts.TileCountX = m_TileCullingBuffers->GetXTileCount();
            screenConsts.TileCountY = m_TileCullingBuffers->GetYTileCount();
            m_CommandList->SetGraphicsRoot32BitConstants(RootParams::ScreenConstants, 4, &screenConsts, 0);

            // Bind shadow map SRV (t6)
            m_CommandList->SetGraphicsRootDescriptorTable(RootParams::ShadowMapSRV,
                                                          m_SRVHeap->GetGPUHandle(m_CascadedShadowMap->GetSRVIndex()));

            // Bind SSAO texture SRV (t7) - blurred output
            m_CommandList->SetGraphicsRootDescriptorTable(RootParams::SSAOSRV,
                                                          m_SRVHeap->GetGPUHandle(m_SSAO->GetSSAOBlurredSRVIndex()));

            // Bind shadow constants CBV (b3)
            m_CommandList->SetGraphicsRootConstantBufferView(RootParams::ShadowCBV,
                                                              m_CascadedShadowMap->GetConstantBuffer()->GetGPUVirtualAddress());

            // Bind tone mapping constants CBV (b4)
            m_CommandList->SetGraphicsRootConstantBufferView(RootParams::ToneMappingConstants,
                                                              m_ToneMappingConstantsBuffer->GetGPUVirtualAddress());

            m_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            m_CommandList->RSSetViewports(1, &m_Viewport);
            m_CommandList->RSSetScissorRects(1, &m_ScissorRect);

            // Prepare default texture indices for scene rendering
            Model::DefaultTextureIndices DefaultTextures{};
            DefaultTextures.Albedo = m_DefaultTexture->GetSRVIndex();
            DefaultTextures.NormalMap = m_DefaultNormalMap->GetSRVIndex();
            DefaultTextures.MetallicRoughness = m_DefaultMetallicRoughness->GetSRVIndex();

            if (m_Scene) {
                m_Scene->Render(m_CommandList.Get(), m_SRVHeap.get(), DefaultTextures);
            }

            // Transition resources back for next frame
            D3D12_RESOURCE_BARRIER EndFrameBarriers[4] = {};

            // Light culling UAVs back to UAV
            EndFrameBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
                m_TileCullingBuffers->GetLightGridResource(),
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            EndFrameBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
                m_TileCullingBuffers->GetLightIndexListResource(),
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

            // Shadow maps back to depth write
            EndFrameBarriers[2] = CD3DX12_RESOURCE_BARRIER::Transition(
                m_CascadedShadowMap->GetResource(),
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_DEPTH_WRITE);

            // SSAO blurred back to UAV
            EndFrameBarriers[3] = CD3DX12_RESOURCE_BARRIER::Transition(
                m_SSAO->GetSSAOBlurred(),
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

            m_CommandList->ResourceBarrier(4, EndFrameBarriers);
        } {
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
        auto deltaTime = (t1 - t0).count() * 1e-9f;
        t0 = t1;

        elapsedSeconds += deltaTime;
        if (elapsedSeconds > 1.0) {
            char buffer[500];
            auto fps = frameCounter / elapsedSeconds;
            printf("FPS: %f\n", fps);

            frameCounter = 0;
            elapsedSeconds = 0.0;

        }




        m_Camera->Update(deltaTime);

        if (m_Scene) {
            m_Scene->Update(deltaTime);
        }
    }

    void Renderer::CleanUpRenderer() {
        GetDeviceContext().m_CommandSystem->FlushCommands(m_Fence->GetFence(), m_Fence->GetFenceValue(),
                                                          m_Fence->GetFenceEvent());

        if (GetDeviceContext().m_Cleaner) {
            GetDeviceContext().m_Cleaner->Clean();
        }

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
