#include <memory>
#include <windows.h>

#include "Header/Window/HOXWindow.h"
#include "Header/Window/WindowBuilder.h"


int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    try {
        std::unique_ptr<HOX::HOXWindow> Window = std::make_unique<HOX::HOXWindow>();

        auto WindowHandle = std::make_unique<HOX::WindowBuilder>("Window Builder")
                ->SetWindowInstance(hInstance)
                .SetWindowClassName("Window Class")
                .SetWindowTitle("HOX Renderer")
                .SetWindowProc(HOX::HOXWindow::WindowProc)
                .SetWindowStyle(WS_OVERLAPPEDWINDOW)
                .Build();

        ShowWindow(WindowHandle, nCmdShow);
        Window->SetWindowHandle(WindowHandle);
        Window->Run();

    } catch (const std::exception &e) {
        MessageBoxA(nullptr, e.what(), "Fatal Error", MB_OK | MB_ICONERROR);
        return -1;
    } catch (...) {
        MessageBoxA(nullptr, "Unknown fatal error", "Fatal Error", MB_OK | MB_ICONERROR);
        return -1;
    }

    return 0;
}
