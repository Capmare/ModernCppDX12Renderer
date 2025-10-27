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
        .SetWindowProc(HOX::HOXWindow::WindowProc)
        .Build();

        ShowWindow(WindowHandle, nCmdShow);
        Window->SetWindowHandle(WindowHandle);


    } catch (...) {
    }

    return 0;
}
