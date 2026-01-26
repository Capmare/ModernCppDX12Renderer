//
// Created by david on 10/27/2025.
//


module;

module HOX.Window;

import std;
import HOX.Win32;
import HOX.Types;
import HOX.Renderer;
import HOX.WindowBuilder;
import HOX.Context;

namespace HOX {
    Window::Window(const HOX::Win32::HINSTANCE &hInstance, int nCmdShow) {
        m_Renderer = std::make_unique<Renderer>();
        m_Window = std::make_unique<HOX::WindowBuilder>("Window Builder")
                ->SetWindowInstance(hInstance)
                .SetWindowClassName(L"Window Class")
                .SetWindowTitle(L"HOX Renderer")
                .SetWindowProc(WindowThunk)
                .SetWindowStyle(HOX::Win32::WSOverlappedWindow)
                .SetWindowLocationAndSize({m_Xloc, m_Yloc, m_Width, m_Height})
                .Build();

        HOX::Win32::SetWindowLongPtrW_(m_Window, HOX::Win32::GWLPUserData,
            reinterpret_cast<HOX::Win32::LONG_PTR>(this));


        HOX::Win32::ShowWindow_(m_Window, nCmdShow);
    }

    static bool bFirstCall{true};

    HOX::Win32::LRESULT Window::WindowProc(HOX::Win32::HWND hwnd, HOX::Win32::UINT uMsg, HOX::Win32::WPARAM wParam, HOX::Win32::LPARAM lParam) {
        switch (uMsg) {
            case HOX::Win32::MsgDestroy:
                HOX::Win32::PostQuitMessage_(0);
                break;
            case HOX::Win32::MsgPaint:
                m_Renderer->Update();
                m_Renderer->Render();
                break;
            case HOX::Win32::MsgSize:

            {
                if (bFirstCall) {
                    bFirstCall = false;
                    break;
                }

                HOX::Win32::RECT clientRect = {};
                HOX::Win32::GetClientRect_(hwnd, &clientRect);

                const u32 Width = clientRect.right - clientRect.left;
                const u32 Height = clientRect.bottom - clientRect.top;

                m_Renderer->ResizeSwapChain(Width, Height);
            }

                break;
            default:
                return HOX::Win32::DefWindowProcW_(hwnd, uMsg, wParam, lParam);
        }

        return 0;
    }

    void Window::Run() {
        GetDeviceContext().m_WindowWidth = m_Width;
        GetDeviceContext().m_WindowHeight = m_Height;

        m_Renderer->InitializeRenderer(m_Window);
        HOX::Win32::MSG msg{};
        while (msg.message != HOX::Win32::MsgQuit) {
            if (HOX::Win32::PeekMessageW_(&msg, nullptr, 0, 0, HOX::Win32::PMRemove)) {
                HOX::Win32::TranslateMessage_(&msg);
                HOX::Win32::DispatchMessageW_(&msg);
            }
        }
        m_Renderer->CleanUpRenderer();
    }

    std::tuple<int, int, int, int> Window::GetWindowLocationAndDimension() {
        return {m_Xloc, m_Yloc, m_Width, m_Height};
    }

    void Window::SetWindowLocationAndDimension(std::tuple<int, int, int, int> NewLocation) {
        m_Xloc = std::get<0>(NewLocation);
        m_Yloc = std::get<1>(NewLocation);
        m_Width = std::get<2>(NewLocation);
        m_Height = std::get<3>(NewLocation);
    }

    HOX::Win32::LRESULT Window::WindowThunk(HOX::Win32::HWND hwnd, HOX::Win32::UINT uMsg, HOX::Win32::WPARAM wParam, HOX::Win32::LPARAM lParam) {
        Window *pThis = reinterpret_cast<Window *>(HOX::Win32::GetWindowLongPtrW_(hwnd, HOX::Win32::GWLPUserData));
        if (pThis) {
            return pThis->WindowProc(hwnd, uMsg, wParam, lParam);
        }
        return HOX::Win32::DefWindowProcW_(hwnd, uMsg, wParam, lParam);
    }
}
