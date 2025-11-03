//
// Created by david on 10/27/2025.
//

#ifndef MODERNCPPDX12RENDERER_HOXWINDOW_H
#define MODERNCPPDX12RENDERER_HOXWINDOW_H
#include <memory>
#include <Windows.h>
#include "../Renderer/Renderer.h"

namespace HOX {
    class Window {
    public:
        Window(const HINSTANCE& hInstance, int nCmdShow);
        ~Window() = default;

        // Prevent copy and move
        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;
        Window(Window&&) = delete;
        Window& operator=(Window&&) = delete;

        void SetWindowHandle(const HWND& WindowHandle) { m_Window = WindowHandle; }
        static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);


        void Run();
    private:
        std::unique_ptr<HOX::Renderer> m_Renderer;
        bool bShouldQuit{false};
        HWND m_Window{};
    };
}


#endif //MODERNCPPDX12RENDERER_HOXWINDOW_H
