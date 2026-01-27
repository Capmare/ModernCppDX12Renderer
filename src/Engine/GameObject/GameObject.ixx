//
// Created by capma on 27-Jan-26.
//

module;
#include <d3d12.h>
#include <DirectXMath.h>

export module HOX.GameObject;

import std;
import HOX.Types;
import HOX.Win32;
import HOX.Model;
import HOX.Transform;
import HOX.MemoryAllocator;

export namespace HOX {

    using HOX::Win32::ComPtr;

    struct PerObjectConstants {
        DirectX::XMFLOAT4X4 m_WorldMatrix{};
    };

    constexpr u64 PerObjectConstantsAlignedSize{256};

    class GameObject {
    public:
        GameObject() = default;
        virtual ~GameObject();

        GameObject(const GameObject&) = delete;
        GameObject(GameObject&&) noexcept = delete;
        GameObject& operator=(const GameObject&) = default;
        GameObject& operator=(GameObject&&) noexcept = default;


        std::shared_ptr<Model> m_Model{};

        Transform m_Transform{};

        void CreateConstantBuffer();

        void UpdateConstantBuffer() const;

        void Draw(ID3D12GraphicsCommandList *CommandList);

        void Release();

    private:
        BufferAllocation m_ConstantBufferAllocation{};
        void* m_ConstantBufferMapped{nullptr};

    };
}
