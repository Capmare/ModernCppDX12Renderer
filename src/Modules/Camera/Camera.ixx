//
// Created by capma on 26-Jan-26.
//

module;
#include <DirectXMath.h>


export module HOX.Camera;

export namespace HOX {

    struct CameraConstants {
        DirectX::XMFLOAT4X4 m_ViewProjection;
    };

    constexpr size_t CalcConstantBufferSize(size_t Size) {
        return (Size + 255) & ~255; // round up to multiple of 256
    }

    constexpr size_t CameraConstantsSize = CalcConstantBufferSize(sizeof(CameraConstants));

    class Camera {
    public:
        Camera();

        void SetPosition(float X, float Y, float Z);
        void SetPosition(const DirectX::XMFLOAT3& Position);
        void SetRotation(float Pitch, float Yaw);
        void SetProjection(float FovY, float AspectRatio, float ZNear, float ZFar);


        void Update(float DeltaTime);
        [[nodiscard]] const DirectX::XMFLOAT3& GetPosition() const;
        [[nodiscard]] DirectX::XMMATRIX GetRotation() const;
        [[nodiscard]] DirectX::XMMATRIX GetViewMatrix();
        [[nodiscard]] DirectX::XMMATRIX GetProjectionMatrix() const;
        [[nodiscard]] DirectX::XMMATRIX GetViewProjectionMatrix();

        void MoveForward(float Value);
        void MoveRight(float Value);
        void MoveUp(float Value);
        void Rotate(float PitchDelta, float YawDelta);

        void UpdateAspectRatio(float NewAspectRatio);

        const float m_MovementSpeed = 5.0f;
        const float m_MouseSensitivity = 0.001f;
    private:
        void UpdateViewMatrix();
        void UpdateProjectionMatrix();

        DirectX::XMFLOAT3 m_Position{0.f,0.f,-5.f};
        float m_Pitch{};
        float m_Yaw{};

        DirectX::XMFLOAT4X4 m_ViewMatrix{};
        DirectX::XMFLOAT4X4 m_ProjectionMatrix{};

        float m_FovY{DirectX::XM_PIDIV4};
        float m_AspectRatio{16.f/9.f};
        float m_ZNear{0.1f};
        float m_ZFar{100.0f};

        bool m_ViewDirty{true};

    };


}
