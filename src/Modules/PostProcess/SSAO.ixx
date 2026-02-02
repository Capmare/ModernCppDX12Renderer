//
// Screen-Space Ambient Occlusion (SSAO)
//

module;
#include <d3d12.h>
#include <DirectXMath.h>

export module HOX.SSAO;

import std;
import HOX.Types;
import HOX.Win32;
import HOX.DescriptorHeap;

export namespace HOX {
    using HOX::Win32::ComPtr;

    class SSAO {
    public:
        SSAO() = default;
        ~SSAO() = default;

        SSAO(const SSAO&) = delete;
        SSAO& operator=(const SSAO&) = delete;
        SSAO(SSAO&&) noexcept = default;
        SSAO& operator=(SSAO&&) noexcept = default;

        void Initialize(u32 Width, u32 Height, DescriptorHeap* SRVHeap);
        void Resize(u32 Width, u32 Height, DescriptorHeap* SRVHeap);
        void Shutdown();

        // Update constants (call each frame before dispatch)
        void UpdateConstants(
            const DirectX::XMMATRIX& Projection,
            const DirectX::XMMATRIX& InverseProjection,
            u32 ScreenWidth,
            u32 ScreenHeight
        );

        // Get resources for compute dispatch
        [[nodiscard]] ID3D12Resource* GetSSAOOutput() const { return m_SSAOTexture.Get(); }
        [[nodiscard]] ID3D12Resource* GetSSAOBlurred() const { return m_SSAOBlurredTexture.Get(); }
        [[nodiscard]] ID3D12Resource* GetNoiseTexture() const { return m_NoiseTexture.Get(); }
        [[nodiscard]] ID3D12Resource* GetConstantBuffer() const { return m_ConstantBuffer.Get(); }

        // Descriptor indices
        [[nodiscard]] u32 GetSSAOOutputUAVIndex() const { return m_SSAOOutputUAVIndex; }
        [[nodiscard]] u32 GetSSAOOutputSRVIndex() const { return m_SSAOOutputSRVIndex; }
        [[nodiscard]] u32 GetSSAOBlurredUAVIndex() const { return m_SSAOBlurredUAVIndex; }
        [[nodiscard]] u32 GetSSAOBlurredSRVIndex() const { return m_SSAOBlurredSRVIndex; }
        [[nodiscard]] u32 GetNoiseSRVIndex() const { return m_NoiseSRVIndex; }

        // Get constants for shader binding
        [[nodiscard]] const SSAOConstants& GetConstants() const { return m_Constants; }

        // Parameters
        void SetRadius(float Radius) { m_Radius = Radius; }
        void SetBias(float Bias) { m_Bias = Bias; }
        void SetPower(float Power) { m_Power = Power; }

        [[nodiscard]] float GetRadius() const { return m_Radius; }
        [[nodiscard]] float GetBias() const { return m_Bias; }
        [[nodiscard]] float GetPower() const { return m_Power; }

    private:
        void CreateTextures(u32 Width, u32 Height, DescriptorHeap* SRVHeap);
        void CreateNoiseTexture(DescriptorHeap* SRVHeap);
        void GenerateSampleKernel();
        void ReleaseTextures();

        // SSAO output textures
        ComPtr<ID3D12Resource> m_SSAOTexture;         // Raw SSAO output
        ComPtr<ID3D12Resource> m_SSAOBlurredTexture;  // Blurred SSAO

        // Noise texture (4x4 random vectors)
        ComPtr<ID3D12Resource> m_NoiseTexture;

        // Constant buffer
        ComPtr<ID3D12Resource> m_ConstantBuffer;
        void* m_ConstantBufferMapped{};
        SSAOConstants m_Constants{};

        // Descriptor indices
        u32 m_SSAOOutputUAVIndex{};
        u32 m_SSAOOutputSRVIndex{};
        u32 m_SSAOBlurredUAVIndex{};
        u32 m_SSAOBlurredSRVIndex{};
        u32 m_NoiseSRVIndex{};

        // Parameters
        float m_Radius{0.5f};
        float m_Bias{0.025f};
        float m_Power{2.0f};

        u32 m_Width{};
        u32 m_Height{};

        bool m_bInitialized{false};
    };
}
