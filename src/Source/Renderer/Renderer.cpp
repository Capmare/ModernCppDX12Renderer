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

            D3D12_ROOT_PARAMETER RootParameter[9] = {};

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

            // Static sampler for texture filtering
            D3D12_STATIC_SAMPLER_DESC StaticSampler{};
            StaticSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
            StaticSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            StaticSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            StaticSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            StaticSampler.MipLODBias = 0.0f;
            StaticSampler.MaxAnisotropy = 1;
            StaticSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
            StaticSampler.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK;
            StaticSampler.MinLOD = 0.0f;
            StaticSampler.MaxLOD = D3D12_FLOAT32_MAX;
            StaticSampler.ShaderRegister = 0; // s0
            StaticSampler.RegisterSpace = 0;
            StaticSampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

            D3D12_ROOT_SIGNATURE_DESC RootSignatureDesc = {};
            RootSignatureDesc.NumParameters = 9;
            RootSignatureDesc.pParameters = RootParameter;
            RootSignatureDesc.NumStaticSamplers = 1;
            RootSignatureDesc.pStaticSamplers = &StaticSampler;
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

        m_LightManager = std::make_unique<LightManager>();
        m_LightManager->Initialize(m_SRVHeap.get());

        // Add test lights
        {
            GPULight Light{};

            // Directional light (sun) - illuminates entire scene
            Light.m_Type = LightType::Directional;
            Light.m_Direction = {-1.f, -1.f, 0.f}; // Angled down
            Light.m_Color = {1.0f, 0.95f, 0.8f}; // Warm sunlight
            Light.m_Intensity = 1.5f;
            Light.Range = 0.0f; // Not used for directional
            m_LightManager->AddLight(Light);

            // Point lights - scattered around the scene
            Light.m_Type = LightType::Point;
            Light.Range = 50.0f;
            Light.m_Intensity = 2.0f;

            // Center white light
            Light.m_Position = {0.0f, 8.0f, 0.0f};
            Light.m_Color = {1.0f, 1.0f, 1.0f};
            m_LightManager->AddLight(Light);

            // Red light left side
            Light.m_Position = {-15.0f, 4.0f, 0.0f};
            Light.m_Color = {1.0f, 0.3f, 0.3f};
            m_LightManager->AddLight(Light);

            // Blue light right side
            Light.m_Position = {15.0f, 4.0f, 0.0f};
            Light.m_Color = {0.3f, 0.3f, 1.0f};
            m_LightManager->AddLight(Light);

            // Green light back
            Light.m_Position = {0.0f, 4.0f, -15.0f};
            Light.m_Color = {0.3f, 1.0f, 0.3f};
            m_LightManager->AddLight(Light);

            // Yellow light front
            Light.m_Position = {0.0f, 4.0f, 15.0f};
            Light.m_Color = {1.0f, 1.0f, 0.3f};
            m_LightManager->AddLight(Light);

            // Spot lights
            Light.m_Type = LightType::Spot;
            Light.Range = 300.0f;
            Light.m_Intensity = 5.0f;
            Light.m_SpotInnerAngle = 0.3f; // ~17 degrees
            Light.m_SpotOuterAngle = 0.5f; // ~29 degrees

            // Spotlight pointing down from above
            Light.m_Position = {5.0f, 10.0f, 5.0f};
            Light.m_Direction = {0.0f, -1.0f, 0.0f};
            Light.m_Color = {1.0f, 0.8f, 0.6f};
            m_LightManager->AddLight(Light);

            // Spotlight pointing at an angle
            Light.m_Position = {-8.0f, 50.0f, -50.0f};
            Light.m_Direction = {0.5f, -0.5f, 0.5f};
            Light.m_Color = {0.6f, 0.8f, 1.0f};
            m_LightManager->AddLight(Light);

            m_LightManager->UpdateGPUBuffer();
        }

        m_TileCullingBuffers = std::make_unique<TileCullingBuffers>();
        m_TileCullingBuffers->Initialize(
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
            m_ModelLoader->LoadFromFile("../Resources/Sponza/sponza.glb", m_CommandList.Get(), m_SRVHeap.get()));

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

            DepthPSODesc.PS = {nullptr,0};

            DepthPSODesc.NumRenderTargets = 0;
            DepthPSODesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;

            Hr = GetDeviceContext().m_Device->CreateGraphicsPipelineState(
                &DepthPSODesc,
                HOX::Win32::UuidOf<ID3D12PipelineState>(),
                HOX::Win32::PpvArgs(m_DepthPSO.ReleaseAndGetAddressOf()));

            LogD3DCompileFailure("Failed to create depth ");
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

        // Depth pass
        {
            D3D12_CPU_DESCRIPTOR_HANDLE DSV = m_DSVHeap->GetCPUDescriptorHandleForHeapStart();
            m_CommandList->OMSetRenderTargets(0, nullptr, FALSE, &DSV);
            m_CommandList->ClearDepthStencilView(DSV, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

            m_CommandList->SetGraphicsRootSignature(m_RootSignature.Get());
            m_CommandList->SetPipelineState(m_DepthPSO.Get());
            m_CommandList->RSSetViewports(1, &m_Viewport);
            m_CommandList->RSSetScissorRects(1, &m_ScissorRect);
            m_CommandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

            m_CommandList->SetGraphicsRootConstantBufferView(RootParams::CameraCBV,
                                                             m_CameraConstantbuffer->GetGPUVirtualAddress());

            if (m_Scene) {
                m_Scene->DrawDepthOnly(m_CommandList.Get());
            }
        }

        // Light Culling Compute Pass
        {
            // Transition depth buffer from depth write to shader resource
            CD3DX12_RESOURCE_BARRIER DepthBarrier = CD3DX12_RESOURCE_BARRIER::Transition(
                m_DepthStencilBuffer.Get(),
                D3D12_RESOURCE_STATE_DEPTH_WRITE,
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

            // Transition UAVs back for next frame's compute pass
            D3D12_RESOURCE_BARRIER UAVBarriers[2] = {};
            UAVBarriers[0] = CD3DX12_RESOURCE_BARRIER::Transition(
                m_TileCullingBuffers->GetLightGridResource(),
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            UAVBarriers[1] = CD3DX12_RESOURCE_BARRIER::Transition(
                m_TileCullingBuffers->GetLightIndexListResource(),
                D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
                D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
            m_CommandList->ResourceBarrier(2, UAVBarriers);
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
            sprintf_s(buffer, 500, "FPS: %f\n", fps);
            OutputDebugString(buffer);

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
