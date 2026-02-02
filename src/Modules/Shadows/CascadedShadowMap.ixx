//
// Cascaded Shadow Map for directional light shadows
//

module;
#include <d3d12.h>
#include <DirectXMath.h>

export module HOX.CascadedShadowMap;

import std;
import HOX.Types;
import HOX.Win32;
import HOX.DescriptorHeap;

export namespace HOX {
    using HOX::Win32::ComPtr;

    class CascadedShadowMap {
    public:
        CascadedShadowMap() = default;
        ~CascadedShadowMap() = default;

        CascadedShadowMap(const CascadedShadowMap&) = delete;
        CascadedShadowMap& operator=(const CascadedShadowMap&) = delete;
        CascadedShadowMap(CascadedShadowMap&&) noexcept = default;
        CascadedShadowMap& operator=(CascadedShadowMap&&) noexcept = default;

        void Initialize(DescriptorHeap* SRVHeap);
        void Shutdown();

        // Calculate cascade matrices based on camera frustum and light direction
        void Update(
            const DirectX::XMMATRIX& CameraView,
            const DirectX::XMMATRIX& CameraProjection,
            const DirectX::XMFLOAT3& LightDirection,
            float ZNear,
            float ZFar
        );

        // Get DSV handle for rendering to specific cascade
        [[nodiscard]] D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle(u32 CascadeIndex) const;

        // Get SRV index for sampling shadow maps in pixel shader
        [[nodiscard]] u32 GetSRVIndex() const { return m_SRVIndex; }

        // Get shadow map resource for barriers
        [[nodiscard]] ID3D12Resource* GetResource() const { return m_ShadowMapArray.Get(); }

        // Get cascade view-projection matrix
        [[nodiscard]] const DirectX::XMFLOAT4X4& GetCascadeViewProjection(u32 CascadeIndex) const {
            return m_CascadeData[CascadeIndex].ViewProjection;
        }

        // Get cascade split distances (in view space Z)
        [[nodiscard]] DirectX::XMFLOAT4 GetCascadeSplits() const;

        // Get shadow constants for shader
        [[nodiscard]] const ShadowConstants& GetShadowConstants() const { return m_ShadowConstants; }
        [[nodiscard]] ID3D12Resource* GetConstantBuffer() const { return m_ConstantBuffer.Get(); }

        // Set light direction for shadow constants
        void SetLightDirection(const DirectX::XMFLOAT3& LightDir) { m_ShadowConstants.LightDirection = LightDir; }

        // Update constant buffer with current cascade data
        void UpdateConstantBuffer(const DirectX::XMMATRIX& CameraView);

        [[nodiscard]] static constexpr u32 GetShadowMapSize() { return CSM_SHADOW_MAP_SIZE; }
        [[nodiscard]] static constexpr u32 GetNumCascades() { return CSM_NUM_CASCADES; }

    private:
        void CalculateCascadeSplits(float ZNear, float ZFar);
        void CalculateCascadeMatrices(
            const DirectX::XMMATRIX& CameraView,
            const DirectX::XMMATRIX& CameraProjection,
            const DirectX::XMFLOAT3& LightDirection,
            float ZNear,
            float ZFar
        );

        // Get frustum corners in world space for a given depth range
        void GetFrustumCornersWorldSpace(
            const DirectX::XMMATRIX& CameraView,
            const DirectX::XMMATRIX& CameraProjection,
            float NearZ,
            float FarZ,
            DirectX::XMVECTOR* OutCorners
        );

        struct CascadeData {
            DirectX::XMFLOAT4X4 ViewProjection;
            float SplitDistance;
        };

        // Shadow map texture array (4 cascades)
        ComPtr<ID3D12Resource> m_ShadowMapArray;

        // DSV heap for rendering to each cascade
        ComPtr<ID3D12DescriptorHeap> m_DSVHeap;
        u32 m_DSVDescriptorSize{};

        // SRV index in main descriptor heap
        u32 m_SRVIndex{};

        // Per-cascade data
        CascadeData m_CascadeData[CSM_NUM_CASCADES]{};
        float m_CascadeSplits[CSM_NUM_CASCADES + 1]{};

        // Constant buffer for shadow matrices
        ComPtr<ID3D12Resource> m_ConstantBuffer;
        void* m_ConstantBufferMapped{};
        ShadowConstants m_ShadowConstants{};

        // Shadow parameters
        float m_ShadowBias{0.001f};  // Bias for depth comparison (normalized depth)
        float m_MaxShadowDistance{5000.0f};
        float m_CascadeSplitLambda{0.75f}; // Blend between logarithmic and linear splits

        bool m_bInitialized{false};
    };
}
