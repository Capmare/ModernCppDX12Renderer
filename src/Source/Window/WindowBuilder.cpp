//
// Created by david on 10/27/2025.
//


module;

module HOX.WindowBuilder;
import HOX.Win32;
import HOX.Logger;
import std;

namespace HOX {
    WindowBuilder & WindowBuilder::SetWindowProc(const HOX::Win32::WNDPROC &WindowProc) {
        m_WindowClass.lpfnWndProc = WindowProc;
        m_WindowClass.hInstance = m_hInstance;
        m_WindowClass.style = HOX::Win32::CsHRedraw | HOX::Win32::CsVRedraw;
        m_WindowClass.lpszClassName = m_WindowClassName.c_str();
        return *this;
    }

    WindowBuilder& WindowBuilder::SetWindowInstance(const HOX::Win32::HINSTANCE &instance) {
        m_hInstance = instance;
        return *this;
    }

    WindowBuilder& WindowBuilder::SetWindowClassName(const std::wstring &WindowClassName) {
        m_WindowClassName = WindowClassName.c_str();
        return *this;
    }

    WindowBuilder& WindowBuilder::SetWindowTitle(const std::wstring &WindowTitle) {
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

    WindowBuilder & WindowBuilder::SetWindowStyle(const HOX::Win32::DWORD &WindowStyle) {
        m_WindowStyle = WindowStyle;
        return *this;
    }

    HOX::Win32::HWND WindowBuilder::BuildImpl() {
        HOX::Win32::RegisterClassW_(&m_WindowClass);

        HOX::Win32::HWND WindowHandle = HOX::Win32::CreateWindowExW_(
            0,
            m_WindowClassName.data(),
            m_WindowName.data(),
            m_WindowStyle,
            m_XLocation,
            m_YLocation,
            m_Width,
            m_Height,
            nullptr,
            nullptr,
            m_hInstance,
            nullptr
        );

        if (WindowHandle == nullptr) {
            Logger::LogMessage(Severity::Error, "Failed to create Window");
        }
        Logger::LogMessage(Severity::Normal, "WindowHandle create successfully");

        return WindowHandle;

    }
} // HOX
