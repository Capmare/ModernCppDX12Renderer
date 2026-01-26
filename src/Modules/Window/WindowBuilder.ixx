//
// Created by david on 10/27/2025.
//

export module HOX.WindowBuilder;

import HOX.Win32;
import HOX.Builder;
import std;

export namespace HOX {


    class WindowBuilder final : public Builder<HOX::Win32::HWND,WindowBuilder> {

    public:
        WindowBuilder(const std::string& Name) : Builder(Name) {};
        ~WindowBuilder() = default;

        // Prevent copy and move
        WindowBuilder(const WindowBuilder&) = delete;
        WindowBuilder& operator=(const WindowBuilder&) = delete;
        WindowBuilder(WindowBuilder&&) = delete;
        WindowBuilder& operator=(WindowBuilder&&) = delete;

        WindowBuilder& SetWindowProc(const HOX::Win32::WNDPROC& WindowProc);
        WindowBuilder& SetWindowInstance(const HOX::Win32::HINSTANCE& instance);
        WindowBuilder& SetWindowClassName(const std::wstring &WindowClassName);
        WindowBuilder& SetWindowTitle(const std::wstring &WindowTitle);
        WindowBuilder& SetWindowLocationAndSize(const std::tuple<int,int,int,int>& WindowDimensions);
        WindowBuilder& SetWindowStyle(const HOX::Win32::DWORD& WindowStyle);

        HOX::Win32::HWND BuildImpl();

    private:
        HOX::Win32::HINSTANCE m_hInstance{};
        HOX::Win32::WNDCLASSW m_WindowClass{};
        std::wstring m_WindowClassName = L"DefaultWindowClassName";
        std::wstring  m_WindowName{ L"DefaultWindowName" };
        HOX::Win32::DWORD m_WindowStyle{ HOX::Win32::WSOverlappedDefault };
        int m_XLocation{ HOX::Win32::CWUseDefault };
        int m_YLocation{ HOX::Win32::CWUseDefault };
        int m_Width { HOX::Win32::CWUseDefault };
        int m_Height { HOX::Win32::CWUseDefault };



    };
} // HOX

