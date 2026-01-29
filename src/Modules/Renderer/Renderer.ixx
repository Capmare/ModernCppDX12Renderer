//
// Created by david on 10/29/2025.
//

module;
#include <d3d12.h>
#include <dxgi1_5.h>
#include <directxmath.h>

export module HOX.Renderer;

import std;
import HOX.Win32;
import HOX.Types;
import HOX.DeviceManager;
import HOX.SwapChain;
import HOX.Fence;
import HOX.Context;
import HOX.Camera;
import HOX.Scene;
import HOX.ModelLoader;
import HOX.GameObject;
import HOX.Mesh;
import HOX.Model;
import HOX.Texture;
import HOX.DescriptorHeap;
import HOX.LightManager;
import HOX.TileCullingBuffers;

export namespace HOX {

    using HOX::Win32::ComPtr;

    class Renderer {
    public:
        Renderer();
        ~Renderer() = default;

        // Prevent copy and move
        Renderer(const Renderer &) = delete;
        Renderer &operator=(const Renderer &) = delete;
        Renderer(Renderer &&) = default;
        Renderer &operator=(Renderer &&) = default;

        void InitializeRenderer(HWND Hwnd);
        void Render();
        // This should be part of the engine
        void Update();
        void CleanUpRenderer();

        void ResizeSwapChain(const u32 Width, const u32 Height);
    private:
        // Swapchain || Synchronization
        std::unique_ptr<HOX::Fence> m_Fence{};
        void SetFullScreen(HWND Hwnd, bool FullScreen);

        RECT m_WindowRect{};
        bool m_bFullScreen{false};

        // Descriptor heap (descriptor sets in vulkan)
        ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(ComPtr<ID3D12Device10> Device,D3D12_DESCRIPTOR_HEAP_TYPE Type, u32 NumDescriptors);

        // RTV
        void UpdateRenderTarget(ComPtr<ID3D12Device10> Device, ComPtr<IDXGISwapChain4> SwapChain,ComPtr<ID3D12DescriptorHeap> DescriptorHeap);
        ComPtr<ID3D12DescriptorHeap> m_RTVDescriptorHeap{};
        UINT m_RTVDescriptorSize{};

        std::unique_ptr<DeviceManager> m_DeviceManager{};
        ComPtr<ID3D12CommandAllocator> m_CommandAllocators[MaxFrames]{};
        ComPtr<ID3D12GraphicsCommandList7> m_CommandList{};

        std::unique_ptr<Swapchain> m_SwapChain{};

        ComPtr<ID3DBlob> m_ErrorBlob;
        ComPtr<ID3DBlob> m_VertexShaderBlob;
        ComPtr<ID3DBlob> m_PixelShaderBlob;
        ComPtr<ID3DBlob> m_DepthVertexShaderShaderBlob;


        // Passes START

        // Depth pass
        ComPtr<ID3D12PipelineState> m_DepthPSO{};


        // Passes END

        // Scene
        std::unique_ptr<HOX::Scene> m_Scene{};
        std::unique_ptr<HOX::GameObject> m_GO{};
        std::unique_ptr<HOX::ModelLoader> m_ModelLoader{};


        // compute pipeline for light cuyllijng
        ComPtr<ID3D12RootSignature> m_ComputeRootSignature{};
        ComPtr<ID3D12PipelineState> m_LightCullingPipelineState{};
        ComPtr<ID3DBlob> m_LightCullingCSBlob{};

        // Culling constants
        ComPtr<ID3D12Resource> m_CullingConstantsBuffer{};
        void* m_CullingConstantsMapped{};

        // Depth buffer SRV (needed for compute to read)
        u32 m_DepthBufferSRVIndex{};

        std::unique_ptr<LightManager> m_LightManager{};
        std::unique_ptr<TileCullingBuffers> m_TileCullingBuffers{};


        // root signature
        ComPtr<ID3DBlob> SignatureBlob;
        ComPtr<ID3D12RootSignature> m_RootSignature{};

        ComPtr<ID3D12PipelineState> m_PipelineState{};

        // Depth
        ComPtr<ID3D12Resource> m_DepthStencilBuffer{};
        ComPtr<ID3D12DescriptorHeap> m_DSVHeap{};

        void CreateDepthBuffer(u32 Width, u32 Height);
        void UpdateViewPortAndScissor(u32 Width, u32 Height);
        D3D12_VIEWPORT m_Viewport{};
        D3D12_RECT m_ScissorRect{};

        // camera
        std::unique_ptr<HOX::Camera> m_Camera{};
        ComPtr<ID3D12Resource>  m_CameraConstantbuffer{};
        void* m_CameraConstantBufferMapped{nullptr}; // its faster to keep the buffer mapped


        // SRV heap for textures
        std::unique_ptr<DescriptorHeap> m_SRVHeap{};
        std::unique_ptr<Texture> m_DefaultTexture{};      // Default magenta texture for missing albedo
        std::unique_ptr<Texture> m_DefaultNormalMap{};    // Default flat normal map (128, 128, 255)
        std::unique_ptr<Texture> m_DefaultMetallicRoughness{}; // Default non-metallic, medium roughness


        bool m_bTearingSupported{false};

    };
} // HOX

