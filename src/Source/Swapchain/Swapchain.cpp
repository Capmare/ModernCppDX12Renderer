//
// Created by capma on 16-Nov-25.
//

module;
#include <dxgi1_4.h>



module HOX.SwapChain;
import HOX.Win32;
import HOX.Context;
import HOX.Logger;
import std;


namespace HOX {

    void Swapchain::Initialize() {
        m_SwapChain = CreateSwapChain(MaxFrames);
    }

    void Swapchain::Resize(HOX::Fence* CurrentFence, const u32 Width, const u32 Height) {
        if (Width != GetDeviceContext().m_WindowWidth || Height != GetDeviceContext().m_WindowHeight) {
            GetDeviceContext().m_WindowWidth = std::max(1u, Width);
            GetDeviceContext().m_WindowHeight = std::max(1u, Height);

            // Flush commands
            GetDeviceContext().m_CommandSystem->FlushCommands(CurrentFence->GetFence(),CurrentFence->GetFenceValue(),CurrentFence->GetFenceEvent());


            for (size_t idx = 0; idx < MaxFrames; ++idx) {
                m_BackBuffers[idx].Reset();
                m_FrameFenceValues[idx] = m_FrameFenceValues[GetCurrentBackBufferIndex()];

            }
            DXGI_SWAP_CHAIN_DESC SwapChainDesc = {};
            HRESULT Hr = m_SwapChain->GetDesc(&SwapChainDesc);
            if (FAILED(Hr)) {
                Logger::LogMessage(Severity::ErrorNoCrash, "Failed to get swap chain description.");
            }

            Hr = m_SwapChain->ResizeBuffers(
                MaxFrames,GetDeviceContext().m_WindowWidth,GetDeviceContext().m_WindowHeight,
                SwapChainDesc.BufferDesc.Format,SwapChainDesc.Flags
                );

            if (FAILED(Hr)) {
                Logger::LogMessage(Severity::ErrorNoCrash, "Failed to resize swap chain.");
            }

        }
    }


    void Swapchain::UpdateBackBuffer(const ComPtr<ID3D12Resource2> &NewBackBuffer, u8 Location) {
        m_BackBuffers[Location] = NewBackBuffer;
    }

    ComPtr<IDXGISwapChain4> Swapchain::CreateSwapChain(u32 BufferCount) {
        ComPtr<IDXGISwapChain4> SwapChain{};
        ComPtr<IDXGIFactory4> Factory{};

        UINT CreateFactoryFlags = 0;

#ifdef _DEBUG
        CreateFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif

        HRESULT Hr = CreateDXGIFactory2(CreateFactoryFlags, IID_PPV_ARGS(Factory.ReleaseAndGetAddressOf()));
        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to create DXGIFactory2.");
        } else {
            Logger::LogMessage(Severity::Info, "Created DXGIFactory2.");
        }


        DXGI_SWAP_CHAIN_DESC1 SwapChainDesc{};
        SwapChainDesc.Width = GetDeviceContext().m_WindowWidth;
        SwapChainDesc.Height = GetDeviceContext().m_WindowHeight;
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
            GetDeviceContext().m_CommandQueue.Get(),
            GetDeviceContext().Hwnd,
            &SwapChainDesc,
            nullptr,
            nullptr,
            &SwapChain1);
        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to create swap chain.");
        }

        // Disable alt enter
        Hr = Factory->MakeWindowAssociation(GetDeviceContext().Hwnd,DXGI_MWA_NO_ALT_ENTER);
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


} // HOX