//
// Created by david on 10/27/2025.
//

#include "../../Header/Window/Window.h"

#include <memory>
#include "../../Header/Window/WindowBuilder.h"
namespace HOX {

    Window::Window(const HINSTANCE& hInstance, int nCmdShow) {
        auto WindowHandle = std::make_unique<HOX::WindowBuilder>("Window Builder")
               ->SetWindowInstance(hInstance)
               .SetWindowClassName("Window Class")
               .SetWindowTitle("HOX Renderer")
               .SetWindowProc(WindowProc)
               .SetWindowStyle(WS_OVERLAPPEDWINDOW)
               .Build();

        ShowWindow(WindowHandle, nCmdShow);
    }

    LRESULT Window::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
        switch (uMsg) {
            case WM_DESTROY:
                PostQuitMessage(0);
                return 0;
            default:
                return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }
    }

    void Window::Run() {
        m_Renderer = std::make_unique<Renderer>();

        MSG msg{0};
        while (!bShouldQuit) {
            if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
                if (msg.message == WM_QUIT) {
                    bShouldQuit = true;
                }
            }
            else {
                // Do rendering
                m_Renderer->Render();

            }
        }
    }

}
