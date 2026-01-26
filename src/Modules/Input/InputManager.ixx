//
// Created by capma on 26-Jan-26.
//

export module HOX.InputManager;

export namespace HOX {
    struct InputState {
        bool W = false;
        bool A = false;
        bool S = false;
        bool D = false;
        bool E = false;
        bool Q = false;

        float MouseDeltaX = 0.0f;
        float MouseDeltaY = 0.0f;
    };


    class InputManager {
    public:
        InputManager() = default;
        virtual ~InputManager() = default;

        InputManager(const InputManager&) = delete;
        InputManager(InputManager&&) noexcept = delete;
        InputManager& operator=(const InputManager&) = delete;
        InputManager& operator=(InputManager&&) noexcept = delete;

        HOX::InputState m_Input{};
        int m_ScreenCenterX{};
        int m_ScreenCenterY{};
        bool m_MouseCaptured{};

    };

}
