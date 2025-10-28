//
// Created by david on 10/27/2025.
//

#include "../../Header/Window/Window.h"

namespace HOX {
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
            }
        }
    }

}