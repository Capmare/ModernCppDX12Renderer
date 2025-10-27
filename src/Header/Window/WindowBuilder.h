//
// Created by david on 10/27/2025.
//

#ifndef MODERNCPPDX12RENDERER_WINDOW_H
#define MODERNCPPDX12RENDERER_WINDOW_H
#include <windows.h>

#include "../Builder.h"



namespace HOX {

    struct WindowParams {
        int xLoc{};
        int yLoc{};
        int width{};
        int height{};
    };

    class WindowBuilder final : public Builder<HWND,WindowBuilder> {

    public:
        WindowBuilder(const std::string& Name) : Builder(Name) {};
        ~WindowBuilder() = default;

        // Prevent copy and move
        WindowBuilder(const WindowBuilder&) = delete;
        WindowBuilder& operator=(const WindowBuilder&) = delete;
        WindowBuilder(WindowBuilder&&) = delete;
        WindowBuilder& operator=(WindowBuilder&&) = delete;

        WindowBuilder& SetWindowProc(const WNDPROC& WindowProc);
        WindowBuilder& SetWindowInstance(const HINSTANCE& instance);
        WindowBuilder& SetWindowClassName(const std::string& WindowClassName);
        WindowBuilder& SetWindowTitle(const std::string& WindowTitle);
        WindowBuilder& SetWindowLocationAndSize(const WindowParams& WindowParams);
        WindowBuilder& SetWindowStyle(const DWORD& WindowStyle);

        HWND BuildImpl();

    private:
        HINSTANCE m_hInstance{};
        WNDCLASS m_WindowClass{};
        LPCSTR m_WindowClassName{ "DefaultWindowClassName" };
        LPCSTR  m_WindowName{ "DefaultWindowName" };
        DWORD m_WindowStyle{ WS_OVERLAPPED };
        int m_XLocation{ CW_USEDEFAULT };
        int m_YLocation{ CW_USEDEFAULT };
        int m_Width { CW_USEDEFAULT };
        int m_Height { CW_USEDEFAULT };



    };
} // HOX

#endif //MODERNCPPDX12RENDERER_WINDOW_H