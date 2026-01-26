//
// Created by capma on 26-Jan-26.
//
module;
#include <DirectXMath.h>

module HOX.Camera;

import HOX.Context;

namespace HOX {
    Camera::Camera() {
        DirectX::XMMATRIX Identity = DirectX::XMMatrixIdentity();
        DirectX::XMStoreFloat4x4(&m_ViewMatrix, Identity);
        DirectX::XMStoreFloat4x4(&m_ProjectionMatrix, Identity);

        UpdateViewMatrix();
        UpdateProjectionMatrix();
    }

    void Camera::SetPosition(float X, float Y, float Z) {
        m_Position = DirectX::XMFLOAT3(X, Y, Z);
        m_ViewDirty = true;
    }

    void Camera::SetPosition(const DirectX::XMFLOAT3 &Position) {
        m_Position = Position;
        m_ViewDirty = true;
    }

    void Camera::SetRotation(float Pitch, float Yaw) {
        m_Pitch = Pitch;
        m_Yaw = Yaw;

        m_ViewDirty = true;
    }

    void Camera::SetProjection(float FovY, float AspectRatio, float ZNear, float ZFar) {
        m_FovY = FovY;
        m_AspectRatio = AspectRatio;
        m_ZNear = ZNear;
        m_ZFar = ZFar;
        UpdateProjectionMatrix();
    }

    void Camera::Update(float DeltaTime) {


        const auto& InputManager = GetDeviceContext().m_InputManager;
        if (InputManager->m_Input.W)MoveForward(m_MovementSpeed * DeltaTime);
        if (InputManager->m_Input.S)MoveForward(-m_MovementSpeed * DeltaTime);
        if (InputManager->m_Input.A)MoveRight(-m_MovementSpeed * DeltaTime);
        if (InputManager->m_Input.D)MoveRight(m_MovementSpeed * DeltaTime);
        if (InputManager->m_Input.E)MoveUp(m_MovementSpeed * DeltaTime);
        if (InputManager->m_Input.Q)MoveUp(-m_MovementSpeed * DeltaTime);

        // Look
        if (InputManager->m_Input.MouseDeltaX != 0 || InputManager->m_Input.MouseDeltaY != 0) {
            Rotate(
                InputManager->m_Input.MouseDeltaY * m_MouseSensitivity,  // Pitch
                InputManager->m_Input.MouseDeltaX * m_MouseSensitivity   // Yaw
            );
            InputManager->m_Input.MouseDeltaX = 0;
            InputManager->m_Input.MouseDeltaY = 0;
        }
    }

    DirectX::XMMATRIX Camera::GetViewMatrix() {
        if (m_ViewDirty) {
            UpdateViewMatrix();
        }
        return DirectX::XMLoadFloat4x4(&m_ViewMatrix);
    }

    DirectX::XMMATRIX Camera::GetProjectionMatrix() const {
        return DirectX::XMLoadFloat4x4(&m_ProjectionMatrix);
    }

    DirectX::XMMATRIX Camera::GetViewProjectionMatrix() {
        DirectX::XMMATRIX View = GetViewMatrix();
        DirectX::XMMATRIX Projection = GetProjectionMatrix();
        return DirectX::XMMatrixMultiply(View, Projection);
    }

    void Camera::MoveForward(float Value) {
        DirectX::XMVECTOR Forward = DirectX::XMVectorSet(
            sinf(m_Yaw),
            0.f,
            cosf(m_Yaw),
            0.f);

        DirectX::XMVECTOR Position = DirectX::XMLoadFloat3(&m_Position);
        Position = DirectX::XMVectorAdd(Position, DirectX::XMVectorScale(Forward, Value));
        XMStoreFloat3(&m_Position, Position);

        m_ViewDirty = true;
    }

    void Camera::MoveRight(float Value) {

        DirectX::XMVECTOR Right = DirectX::XMVectorSet(
            cosf(m_Yaw),
            0.f,
            -sinf(m_Yaw),
            0.f);

        DirectX::XMVECTOR Position = DirectX::XMLoadFloat3(&m_Position);
        Position = DirectX::XMVectorAdd(Position, DirectX::XMVectorScale(Right, Value));
        XMStoreFloat3(&m_Position, Position);

        m_ViewDirty = true;
    }

    void Camera::MoveUp(float Value) {
        m_Position.y += Value;
        m_ViewDirty = true;
    }

    void Camera::Rotate(float PitchDelta, float YawDelta) {
        m_Pitch += PitchDelta;
        m_Yaw += YawDelta;

        const float MaxPitch = DirectX::XM_PIDIV2 - 0.01f;
        if (m_Pitch > MaxPitch) { m_Pitch = MaxPitch; }
        if (m_Pitch < -MaxPitch) { m_Pitch = -MaxPitch; }

        m_ViewDirty = true;
    }

    void Camera::UpdateAspectRatio(float NewAspectRatio) {
        m_AspectRatio = NewAspectRatio;
        UpdateProjectionMatrix();
    }

    // Calculations are done on the XMMATRIX since its SIMD optimized and 16 byte aligned and use XMFLOAT4X4 for storage since we do not need aligment for that
    void Camera::UpdateViewMatrix() {
        const DirectX::XMMATRIX Rotation = DirectX::XMMatrixRotationRollPitchYaw(m_Pitch, m_Yaw, 0.f);

        DirectX::XMVECTOR Forward = DirectX::XMVectorSet(0, 0, 1, 0);
        DirectX::XMVECTOR Up = DirectX::XMVectorSet(0, 1, 0, 0);

        Forward = XMVector3Transform(Forward, Rotation);
        Up = XMVector3Transform(Up, Rotation);

        const DirectX::XMVECTOR Position = XMLoadFloat3(&m_Position);
        const DirectX::XMVECTOR Target = DirectX::XMVectorAdd(Position, Forward);
        const DirectX::XMMATRIX View = DirectX::XMMatrixLookAtLH(Position, Target, Up);

        XMStoreFloat4x4(&m_ViewMatrix, View);
        m_ViewDirty = false;

    }

    void Camera::UpdateProjectionMatrix() {
        DirectX::XMMATRIX Projection = DirectX::XMMatrixPerspectiveFovLH(
            m_FovY,
            m_AspectRatio,
            m_ZNear,
            m_ZFar);

        XMStoreFloat4x4(&m_ProjectionMatrix, Projection);
    }
}
