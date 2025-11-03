//
// Created by david on 10/29/2025.
//

#ifndef MODERNCPPDX12RENDERER_RENDERER_H
#define MODERNCPPDX12RENDERER_RENDERER_H



#include <memory>
#include "../ResourceManagement/DeviceContext.h"

// DirectX12 specific includes
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>


namespace HOX {

    class Renderer {
    public:
        Renderer();
        ~Renderer() = default;

        // Prevent copy and move
        Renderer(const Renderer&) = delete;
        Renderer& operator=(const Renderer&) = delete;
        Renderer(Renderer&&) = delete;
        Renderer& operator=(Renderer&&) = delete;

        void InitializeRenderer();
        void Render();
        void CleanUpRenderer();
    private:
        std::unique_ptr<DeviceContext> m_DeviceContext{};

        const uint8_t m_MaxFrames{3};
        const bool m_bUseWarps{false};


    };
} // HOX

#endif //MODERNCPPDX12RENDERER_RENDERER_H
