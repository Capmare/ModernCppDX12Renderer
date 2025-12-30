//
// Created by david on 10/27/2025.
//

#include "../../Header/Window/Window.h"

#include <memory>
#include "../../Header/Window/WindowBuilder.h"

namespace HOX {
    Window::Window(const HINSTANCE &hInstance, int nCmdShow) {
        m_Renderer = std::make_unique<Renderer>();
        m_Window = std::make_unique<HOX::WindowBuilder>("Window Builder")
                ->SetWindowInstance(hInstance)
                .SetWindowClassName("Window Class")
                .SetWindowTitle("HOX Renderer")
                .SetWindowProc(WindowThunk)
                .SetWindowStyle(WS_OVERLAPPEDWINDOW)
                .SetWindowLocationAndSize({m_Xloc, m_Yloc, m_Width, m_Height})
                .Build();

        SetWindowLongPtr(m_Window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));


        ShowWindow(m_Window, nCmdShow);
    }

    static bool bFirstCall{true};

    LRESULT Window::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
            case WM_DESTROY:
                PostQuitMessage(0);
                break;
            case WM_PAINT:
                m_Renderer->Update();
                m_Renderer->Render();
                break;
            case WM_SIZE:

            {
                if (bFirstCall) {
                    bFirstCall = false;
                    break;
                }

                RECT clientRect = {};
                GetClientRect(hwnd, &clientRect);

                uint32_t Width = clientRect.right - clientRect.left;
                uint32_t Height = clientRect.bottom - clientRect.top;

                m_Renderer->ResizeSwapChain(Width, Height);
            }

                break;
            default:
                return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }

        return 0;
    }

    void Window::Run() {
        GetDeviceContext().m_WindowWidth = m_Width;
        GetDeviceContext().m_WindowHeight = m_Height;

        m_Renderer->InitializeRenderer(m_Window);
        MSG msg{};
        while (msg.message != WM_QUIT) {
            if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
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

    LRESULT Window::WindowThunk(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        Window *pThis = reinterpret_cast<Window *>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
        if (pThis) {
            return pThis->WindowProc(hwnd, uMsg, wParam, lParam);
        }
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}
