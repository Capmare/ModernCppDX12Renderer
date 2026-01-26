//
// Created by david on 10/28/2025.
//

// Exports
module;
#include <d3d12.h>
#include <dxgi1_6.h>

import std;
import HOX.Win32;
import HOX.Types;

export module HOX.Context;
export namespace HOX { inline constexpr std::size_t MaxFrames = 3; }

export import HOX.CommandSystem;
export import HOX.Cleaner;



export namespace HOX {
    using HOX::Win32::ComPtr;

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


        u32 m_WindowWidth{0};
        u32 m_WindowHeight{0};

        bool m_bUseVSync{false};
        bool m_bTearingSupported{false};


    private:

    };


    Context& GetDeviceContext();

} // HOX

