#include <memory>
#include <windows.h>

#include "Header/Window/Window.h"
#include "Header/Window/WindowBuilder.h"


int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    try {
        std::unique_ptr<HOX::Window> Window = std::make_unique<HOX::Window>(hInstance, nCmdShow);
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
