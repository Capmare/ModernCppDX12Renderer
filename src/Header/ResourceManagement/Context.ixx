//
// Created by david on 10/28/2025.
//

// Exports
module;
#include <windows.h>
#include <wrl/client.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <cstdint>

export module HOX.Context;
import std;

export namespace HOX { inline constexpr std::size_t MaxFrames = 3; }

export import HOX.CommandSystem;
export import HOX.Cleaner;



export namespace HOX {
    using Microsoft::WRL::ComPtr;

    class Context {
    public:
        Context();
        ~Context();

        // Prevent copy and move
        Context(const Context&) = delete;
        Context& operator=(const Context&) = delete;
        Context(Context&&) = delete;
        Context& operator=(Context&&) = delete;

        HWND Hwnd{};

        ComPtr<ID3D12Device10> m_Device{};
        ComPtr<IDXGIAdapter4> m_Adapter{};
        ComPtr<ID3D12CommandQueue> m_CommandQueue{};

        std::unique_ptr<HOX::Cleaner> m_Cleaner{};
        std::unique_ptr<HOX::CommandSystem> m_CommandSystem{};


        uint32_t m_WindowWidth{0};
        uint32_t m_WindowHeight{0};

        bool m_bUseVSync{false};
        bool m_bTearingSupported{false};


    private:

    };


    Context& GetDeviceContext();

} // HOX

