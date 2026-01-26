//
// Created by capma on 26-Jan-26.
//

module;
#include <DirectXMath.h>

export module HOX.Transform;

export namespace HOX {
    class Transform {
    public:
        DirectX::XMFLOAT3 Position{};
        DirectX::XMFLOAT4 Rotation{0.f, 0.f, 0.f, 1.f};
        DirectX::XMFLOAT3 Scale{1.f, 1.f, 1.f};

        DirectX::XMMATRIX GetWorldMatrix() const {
            DirectX::XMMATRIX ScaleMatrix = DirectX::XMMatrixScaling(Scale.x, Scale.y, Scale.z);
            DirectX::XMMATRIX RotationMatrix = DirectX::XMMatrixRotationQuaternion(DirectX::XMLoadFloat4(&Rotation));
            DirectX::XMMATRIX TranslationMatrix = DirectX::XMMatrixTranslation(Position.x, Position.y, Position.z);

            // SRT Order
            return DirectX::XMMatrixMultiply(DirectX::XMMatrixMultiply(ScaleMatrix,RotationMatrix),TranslationMatrix);
        };

        void SetRotationEuler(float Pitch, float Yawn, float Roll) {
            DirectX::XMVECTOR Quat = DirectX::XMQuaternionRotationRollPitchYaw(Pitch, Yawn, Roll);
            DirectX::XMStoreFloat4(&Rotation, Quat);

        }

    };
}
