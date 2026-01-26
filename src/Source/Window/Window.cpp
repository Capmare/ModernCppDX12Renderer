//
// Created by david on 10/27/2025.
//


module;
#include "../../d3dx12.h"

module HOX.Window;

import std;
import HOX.Win32;
import HOX.Types;
import HOX.Renderer;
import HOX.WindowBuilder;
import HOX.Context;
import HOX.InputManager;

namespace HOX {



    Window::Window(const HOX::Win32::HINSTANCE &hInstance, int nCmdShow) {
        m_Renderer = std::make_unique<Renderer>();
        m_Window = std::make_unique<HOX::WindowBuilder>("Window Builder")
                ->SetWindowInstance(hInstance)
                .SetWindowClassName(L"Window Class")
                .SetWindowTitle(L"HOX Renderer")
                .SetWindowProc(WindowThunk)
                .SetWindowStyle(HOX::Win32::WSOverlappedWindow)
                .SetWindowLocationAndSize({m_Xloc, m_Yloc, m_Width, m_Height})
                .Build();

        HOX::Win32::SetWindowLongPtrW_(m_Window, HOX::Win32::GWLPUserData,
            reinterpret_cast<HOX::Win32::LONG_PTR>(this));


        HOX::Win32::ShowWindow_(m_Window, nCmdShow);
    }

    static bool bFirstCall{true};

    HOX::Win32::LRESULT Window::WindowProc(HOX::Win32::HWND hwnd, HOX::Win32::UINT uMsg, HOX::Win32::WPARAM wParam, HOX::Win32::LPARAM lParam) {
        auto& InputManager = GetDeviceContext().m_InputManager;
        switch (uMsg) {
            case HOX::Win32::MsgDestroy:
                HOX::Win32::PostQuitMessage_(0);
                break;
            case HOX::Win32::MsgPaint:
                m_Renderer->Update();
                m_Renderer->Render();
                break;
            case HOX::Win32::MsgSize:
            {
                if (bFirstCall) {
                    bFirstCall = false;
                    break;
                }

                HOX::Win32::RECT clientRect = {};
                HOX::Win32::GetClientRect_(hwnd, &clientRect);

                const u32 Width = clientRect.right - clientRect.left;
                const u32 Height = clientRect.bottom - clientRect.top;

                m_Renderer->ResizeSwapChain(Width, Height);
            }

                break;
            case WM_KEYDOWN:
                if (wParam == 'W') InputManager->m_Input.W = true;
                if (wParam == 'A') InputManager->m_Input.A = true;
                if (wParam == 'S') InputManager->m_Input.S = true;
                if (wParam == 'D') InputManager->m_Input.D = true;
                if (wParam == 'E') InputManager->m_Input.E = true;
                if (wParam == 'Q') InputManager->m_Input.Q = true;
                break;
            case WM_KEYUP:
                if (wParam == 'W') InputManager->m_Input.W = false;
                if (wParam == 'A') InputManager->m_Input.A = false;
                if (wParam == 'S') InputManager->m_Input.S = false;
                if (wParam == 'D') InputManager->m_Input.D = false;
                if (wParam == 'E') InputManager->m_Input.E = false;
                if (wParam == 'Q') InputManager->m_Input.Q = false;
                break;
            case WM_RBUTTONDOWN:
                InputManager->m_MouseCaptured = !InputManager->m_MouseCaptured;

                if (InputManager->m_MouseCaptured) {
                    // Hide cursor and lock to window
                    ShowCursor(FALSE);

                    // Get window center in screen coordinates
                    RECT rect;
                    GetClientRect(hwnd, &rect);
                    POINT center = { rect.right / 2, rect.bottom / 2 };
                    ClientToScreen(hwnd, &center);

                    // Store center for later use
                    InputManager->m_ScreenCenterX = center.x;
                    InputManager->m_ScreenCenterY = center.y;

                    // Move cursor to center
                    SetCursorPos(InputManager->m_ScreenCenterX, InputManager->m_ScreenCenterY);
                    InputManager->m_Input.MouseDeltaX = 0;
                    InputManager->m_Input.MouseDeltaY = 0;
                    SetCursorPos(InputManager->m_ScreenCenterX, InputManager->m_ScreenCenterY);

                } else {
                    // Show cursor again
                    ShowCursor(TRUE);
                }
                break;
            case WM_MOUSEMOVE:
                if (InputManager->m_MouseCaptured) {
                    int x = HOX::Win32::GetXLParam(lParam);
                    int y = HOX::Win32::GetYLParam(lParam);

                    // Calculate delta from center
                    int centerX = m_Width / 2;
                    int centerY = m_Height / 2;

                    int deltaX = x - centerX;
                    int deltaY = y - centerY;

                    if (deltaX != 0 || deltaY != 0) {
                        InputManager->m_Input.MouseDeltaX += static_cast<float>(deltaX);
                        InputManager->m_Input.MouseDeltaY += static_cast<float>(deltaY);

                        POINT clientCenter = { centerX, centerY };
                        ClientToScreen(hwnd, &clientCenter);
                        SetCursorPos(clientCenter.x, clientCenter.y);
                    }

                }
                break;
            default:
                return HOX::Win32::DefWindowProcW_(hwnd, uMsg, wParam, lParam);
        }

        return 0;
    }

    void Window::Run() {
        GetDeviceContext().m_WindowWidth = m_Width;
        GetDeviceContext().m_WindowHeight = m_Height;


        m_Renderer->InitializeRenderer(m_Window);
        HOX::Win32::MSG msg{};
        while (msg.message != HOX::Win32::MsgQuit) {
            if (HOX::Win32::PeekMessageW_(&msg, nullptr, 0, 0, HOX::Win32::PMRemove)) {
                HOX::Win32::TranslateMessage_(&msg);
                HOX::Win32::DispatchMessageW_(&msg);
            }
        }
        m_Renderer->CleanUpRenderer();
    }

    std::tuple<int, int, int, int> Window::GetWindowLocationAndDimension() {
        return {m_Xloc, m_Yloc, m_Width, m_Height};
    }

    void Window::SetWindowLocationAndDimension(std::tuple<int, int, int, int> NewLocation) {
        m_Xloc = std::get<0>(NewLocation);
        m_Yloc = std::get<1>(NewLocation);
        m_Width = std::get<2>(NewLocation);
        m_Height = std::get<3>(NewLocation);
    }

    HOX::Win32::LRESULT Window::WindowThunk(HOX::Win32::HWND hwnd, HOX::Win32::UINT uMsg, HOX::Win32::WPARAM wParam, HOX::Win32::LPARAM lParam) {
        Window *pThis = reinterpret_cast<Window *>(HOX::Win32::GetWindowLongPtrW_(hwnd, HOX::Win32::GWLPUserData));
        if (pThis) {
            return pThis->WindowProc(hwnd, uMsg, wParam, lParam);
        }
        return HOX::Win32::DefWindowProcW_(hwnd, uMsg, wParam, lParam);
    }

    void Window::UpdateScreenCenter(HOX::Win32::HWND Hwnd) {
        auto& InputManager = GetDeviceContext().m_InputManager;

        RECT Rect{};
        GetClientRect(Hwnd, &Rect);

        POINT Center{};
        Center.x = Rect.right * .5;
        Center.y = Rect.bottom * .5;

        ClientToScreen(Hwnd, &Center);

        InputManager->m_ScreenCenterX = Center.x;
        InputManager->m_ScreenCenterY = Center.y;
    }
}
