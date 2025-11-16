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

        LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);


        void Run();

        std::tuple<int, int, int, int> GetWindowLocationAndDimension();
        void SetWindowLocationAndDimension(std::tuple<int, int, int,int> NewLocation);
    private:
        static LRESULT CALLBACK WindowThunk(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

        std::unique_ptr<HOX::Renderer> m_Renderer;
        HWND m_Window{};

        int m_Xloc{300};
        int m_Yloc{300};
        int m_Width{1920};
        int m_Height{1080};

        bool bShouldQuit{false};
    };
}


#endif //MODERNCPPDX12RENDERER_HOXWINDOW_H
