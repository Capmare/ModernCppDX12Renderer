//
// Created by david on 10/27/2025.
//

#ifndef MODERNCPPDX12RENDERER_HOXWINDOW_H
#define MODERNCPPDX12RENDERER_HOXWINDOW_H
#include <Windows.h>

namespace HOX {
    class HOXWindow {
    public:
        HOXWindow() = default;
        ~HOXWindow() = default;

        // Prevent copy and move
        HOXWindow(const HOXWindow&) = delete;
        HOXWindow& operator=(const HOXWindow&) = delete;
        HOXWindow(HOXWindow&&) = delete;
        HOXWindow& operator=(HOXWindow&&) = delete;

        void SetWindowHandle( HWND& WindowHandle) { m_Window = WindowHandle; }
        static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);


    private:

        HWND m_Window{};
    };
}


#endif //MODERNCPPDX12RENDERER_HOXWINDOW_H
