//
// Created by david on 10/27/2025.
//


module;
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>


module HOX.WindowBuilder;
import HOX.Logger;
import std;

namespace HOX {
    WindowBuilder & WindowBuilder::SetWindowProc(const WNDPROC &WindowProc) {
        m_WindowClass.lpfnWndProc = WindowProc;
        m_WindowClass.hInstance = m_hInstance;
        m_WindowClass.style = CS_HREDRAW | CS_VREDRAW;
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

    WindowBuilder& WindowBuilder::SetWindowLocationAndSize(const std::tuple<int,int,int,int> &WindowDimensions) {
        m_XLocation = std::get<0>(WindowDimensions);
        m_YLocation = std::get<1>(WindowDimensions);
        m_Width = std::get<2>(WindowDimensions);
        m_Height = std::get<3>(WindowDimensions);
        return *this;
    }

    WindowBuilder & WindowBuilder::SetWindowStyle(const DWORD &WindowStyle) {
        m_WindowStyle = WindowStyle;
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
            Logger::LogMessage(Severity::Error, "Failed to create Window");
        }
        Logger::LogMessage(Severity::Normal, "WindowHandle create successfully");

        return WindowHandle;

    }
} // HOX
