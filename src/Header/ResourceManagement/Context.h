//
// Created by david on 10/28/2025.
//

#ifndef MODERNCPPDX12RENDERER_DEVICECONTEXT_H
#define MODERNCPPDX12RENDERER_DEVICECONTEXT_H
#include <memory>
#include "../../pch.h"

#include "Cleaner.h"

namespace HOX {

    class Context {
    public:
        Context();
        ~Context() = default;

        // Prevent copy and move
        Context(const Context&) = delete;
        Context& operator=(const Context&) = delete;
        Context(Context&&) = delete;
        Context& operator=(Context&&) = delete;

        HWND Hwnd{};

        ComPtr<ID3D12Device10> m_Device{};
        ComPtr<IDXGIAdapter4> m_Adapter{};
        ComPtr<ID3D12CommandQueue> m_CommandQueue{};

        std::unique_ptr<HOX::Cleaner> m_Cleaner;
        std::unique_ptr<HOX::CommandSystem> m_CommandSystem{};


        uint32_t m_WindowWidth{0};
        uint32_t m_WindowHeight{0};

        bool m_bUseVSync{false};
        bool m_bTearingSupported{false};

    private:

    };


    Context& GetDeviceContext();

} // HOX

#endif //MODERNCPPDX12RENDERER_DEVICECONTEXT_H
