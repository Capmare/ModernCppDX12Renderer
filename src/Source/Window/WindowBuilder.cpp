//
// Created by david on 10/27/2025.
//

#include "../../Header/Window/WindowBuilder.h"

#include <format>
#include <stdexcept>

namespace HOX {
    WindowBuilder & WindowBuilder::SetWindowProc(const WNDPROC &WindowProc) {
        m_WindowClass.lpfnWndProc = WindowProc;
        m_WindowClass.hInstance = m_hInstance;
        m_WindowClass.lpszClassName = m_WindowClassName;
        return *this;
    }

    WindowBuilder& WindowBuilder::SetWindowInstance(const HINSTANCE &instance) {
        m_hInstance = instance;
        return *this;
    }

    WindowBuilder& WindowBuilder::SetWindowClassName(const std::string &WindowClassName) {
        m_WindowClassName = WindowClassName.c_str();
        return *this;
    }

    WindowBuilder& WindowBuilder::SetWindowTitle(const std::string &WindowTitle) {
        m_WindowName = WindowTitle.c_str();
        return *this;
    }

    WindowBuilder& WindowBuilder::SetWindowLocationAndSize(const WindowParams &WindowParams) {
        m_Width = WindowParams.width;
        m_Height = WindowParams.height;
        m_XLocation = WindowParams.xLoc;
        m_YLocation = WindowParams.yLoc;
        return *this;
    }

    HWND WindowBuilder::BuildImpl() {
        RegisterClass(&m_WindowClass);

        HWND WindowHandle = CreateWindowEx(
            0,
            m_WindowClassName,
            m_WindowName,
            m_WindowStyle,
            m_XLocation,
            m_YLocation,
            m_Width,
            m_Height,
            NULL,
            NULL,
            m_hInstance,
            NULL
        );

        if (WindowHandle == NULL) {
            throw std::logic_error(
                std::format("{} could not create the window with the current settings {}:{}",
                    GetName(),
                    __FILE__, __LINE__)
            );
        }

        return WindowHandle;

    }
} // HOX
