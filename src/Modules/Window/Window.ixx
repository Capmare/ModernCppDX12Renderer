//
// Created by david on 10/27/2025.
//


export module HOX.Window;

import HOX.Win32;
import std;
import HOX.Renderer;
import HOX.InputManager;





export namespace HOX {

    class Window {
    public:
        Window(const HOX::Win32::HINSTANCE& hInstance, int nCmdShow);
        ~Window() = default;

        // Prevent copy and move
        Window(const Window&) = delete;
        Window& operator=(const Window&) = delete;
        Window(Window&&) = delete;
        Window& operator=(Window&&) = delete;

        HOX::Win32::LRESULT WindowProc(HOX::Win32::HWND hwnd, HOX::Win32::UINT uMsg,HOX::Win32::WPARAM wParam, HOX::Win32::LPARAM lParam);


        void Run();

        std::tuple<int, int, int, int> GetWindowLocationAndDimension();
        void SetWindowLocationAndDimension(std::tuple<int, int, int,int> NewLocation);

    private:


        static HOX::Win32::LRESULT WindowThunk(HOX::Win32::HWND hwnd, HOX::Win32::UINT uMsg, HOX::Win32::WPARAM wParam, HOX::Win32::LPARAM lParam);
        void UpdateScreenCenter(HOX::Win32::HWND Hwnd);

        std::unique_ptr<HOX::Renderer> m_Renderer;

        HOX::Win32::HWND m_Window{};

        int m_Xloc{300};
        int m_Yloc{300};
        int m_Width{1920};
        int m_Height{1080};

        bool bShouldQuit{false};
    };
}



