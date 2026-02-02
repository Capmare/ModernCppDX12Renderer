//
// SSAO Implementation
//

module;
#include <d3d12.h>
#include <DirectXMath.h>
#include <random>
#include <cmath>

module HOX.SSAO;

import HOX.Context;
import HOX.Win32;
import HOX.Logger;

namespace HOX {

    void SSAO::Initialize(u32 Width, u32 Height, DescriptorHeap* SRVHeap) {
        if (m_bInitialized) return;

        m_Width = Width;
        m_Height = Height;

        GenerateSampleKernel();
        CreateTextures(Width, Height, SRVHeap);
        CreateNoiseTexture(SRVHeap);

        // Create constant buffer
        auto Device = GetDeviceContext().m_Device.Get();

        D3D12_HEAP_PROPERTIES HeapProps = {};
        HeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC CBDesc = {};
        CBDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        CBDesc.Width = (sizeof(SSAOConstants) + 255) & ~255; // 256-byte aligned
        CBDesc.Height = 1;
        CBDesc.DepthOrArraySize = 1;
        CBDesc.MipLevels = 1;
        CBDesc.SampleDesc.Count = 1;
        CBDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        HRESULT Hr = Device->CreateCommittedResource(
            &HeapProps,
            D3D12_HEAP_FLAG_NONE,
            &CBDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            Win32::UuidOf<ID3D12Resource>(),
            Win32::PpvArgs(m_ConstantBuffer.ReleaseAndGetAddressOf())
        );

        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to create SSAO constant buffer");
            return;
        }

        D3D12_RANGE ReadRange = {0, 0};
        m_ConstantBuffer->Map(0, &ReadRange, &m_ConstantBufferMapped);

        m_bInitialized = true;
        Logger::LogMessage(Severity::Info, "SSAO initialized");
    }

    void SSAO::Resize(u32 Width, u32 Height, DescriptorHeap* SRVHeap) {
        if (Width == m_Width && Height == m_Height) return;

        m_Width = Width;
        m_Height = Height;

        ReleaseTextures();
        CreateTextures(Width, Height, SRVHeap);
    }

    void SSAO::Shutdown() {
        if (m_ConstantBufferMapped) {
            m_ConstantBuffer->Unmap(0, nullptr);
            m_ConstantBufferMapped = nullptr;
        }
        ReleaseTextures();
        m_NoiseTexture.Reset();
        m_ConstantBuffer.Reset();
        m_bInitialized = false;
    }

    void SSAO::GenerateSampleKernel() {
        std::default_random_engine generator;
        std::uniform_real_distribution<float> randomFloats(0.0f, 1.0f);

        for (u32 i = 0; i < SSAO_KERNEL_SIZE; i++) {
            // Random point in hemisphere
            DirectX::XMFLOAT4 sample(
                randomFloats(generator) * 2.0f - 1.0f,  // x: [-1, 1]
                randomFloats(generator) * 2.0f - 1.0f,  // y: [-1, 1]
                randomFloats(generator),                 // z: [0, 1] (hemisphere)
                0.0f
            );

            // Normalize
            DirectX::XMVECTOR vec = DirectX::XMLoadFloat4(&sample);
            vec = DirectX::XMVector3Normalize(vec);

            // Scale to distribute samples closer to the origin
            float scale = static_cast<float>(i) / SSAO_KERNEL_SIZE;
            scale = 0.1f + scale * scale * (1.0f - 0.1f); // lerp(0.1, 1.0, scale^2)
            vec = DirectX::XMVectorScale(vec, scale);

            DirectX::XMStoreFloat4(&m_Constants.Samples[i], vec);
        }
    }

    void SSAO::CreateTextures(u32 Width, u32 Height, DescriptorHeap* SRVHeap) {
        auto Device = GetDeviceContext().m_Device.Get();

        // SSAO output texture (R8_UNORM)
        D3D12_RESOURCE_DESC TexDesc = {};
        TexDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        TexDesc.Width = Width;
        TexDesc.Height = Height;
        TexDesc.DepthOrArraySize = 1;
        TexDesc.MipLevels = 1;
        TexDesc.Format = DXGI_FORMAT_R8_UNORM;
        TexDesc.SampleDesc.Count = 1;
        TexDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        TexDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        D3D12_HEAP_PROPERTIES HeapProps = {};
        HeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        // Create SSAO output texture
        HRESULT Hr = Device->CreateCommittedResource(
            &HeapProps,
            D3D12_HEAP_FLAG_NONE,
            &TexDesc,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            nullptr,
            Win32::UuidOf<ID3D12Resource>(),
            Win32::PpvArgs(m_SSAOTexture.ReleaseAndGetAddressOf())
        );

        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to create SSAO output texture");
            return;
        }

        // Create blurred SSAO texture
        Hr = Device->CreateCommittedResource(
            &HeapProps,
            D3D12_HEAP_FLAG_NONE,
            &TexDesc,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            nullptr,
            Win32::UuidOf<ID3D12Resource>(),
            Win32::PpvArgs(m_SSAOBlurredTexture.ReleaseAndGetAddressOf())
        );

        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to create SSAO blurred texture");
            return;
        }

        // Create UAV and SRV for SSAO output
        m_SSAOOutputUAVIndex = SRVHeap->Allocate();
        D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
        UAVDesc.Format = DXGI_FORMAT_R8_UNORM;
        UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        UAVDesc.Texture2D.MipSlice = 0;
        Device->CreateUnorderedAccessView(m_SSAOTexture.Get(), nullptr, &UAVDesc,
            SRVHeap->GetCPUHandle(m_SSAOOutputUAVIndex));

        m_SSAOOutputSRVIndex = SRVHeap->Allocate();
        D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
        SRVDesc.Format = DXGI_FORMAT_R8_UNORM;
        SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        SRVDesc.Texture2D.MipLevels = 1;
        Device->CreateShaderResourceView(m_SSAOTexture.Get(), &SRVDesc,
            SRVHeap->GetCPUHandle(m_SSAOOutputSRVIndex));

        // Create UAV and SRV for blurred SSAO
        m_SSAOBlurredUAVIndex = SRVHeap->Allocate();
        Device->CreateUnorderedAccessView(m_SSAOBlurredTexture.Get(), nullptr, &UAVDesc,
            SRVHeap->GetCPUHandle(m_SSAOBlurredUAVIndex));

        m_SSAOBlurredSRVIndex = SRVHeap->Allocate();
        Device->CreateShaderResourceView(m_SSAOBlurredTexture.Get(), &SRVDesc,
            SRVHeap->GetCPUHandle(m_SSAOBlurredSRVIndex));
    }

    void SSAO::CreateNoiseTexture(DescriptorHeap* SRVHeap) {
        auto Device = GetDeviceContext().m_Device.Get();

        // 4x4 noise texture with random rotation vectors
        constexpr u32 NoiseSize = 4;

        std::default_random_engine generator;
        std::uniform_real_distribution<float> randomFloats(0.0f, 1.0f);

        // Generate noise data (RGBA32 float)
        DirectX::XMFLOAT4 noiseData[NoiseSize * NoiseSize];
        for (u32 i = 0; i < NoiseSize * NoiseSize; i++) {
            noiseData[i] = DirectX::XMFLOAT4(
                randomFloats(generator) * 2.0f - 1.0f,
                randomFloats(generator) * 2.0f - 1.0f,
                0.0f,
                0.0f
            );
            // Normalize xy
            float len = std::sqrt(noiseData[i].x * noiseData[i].x + noiseData[i].y * noiseData[i].y);
            if (len > 0.0001f) {
                noiseData[i].x /= len;
                noiseData[i].y /= len;
            }
        }

        // Create noise texture
        D3D12_RESOURCE_DESC TexDesc = {};
        TexDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        TexDesc.Width = NoiseSize;
        TexDesc.Height = NoiseSize;
        TexDesc.DepthOrArraySize = 1;
        TexDesc.MipLevels = 1;
        TexDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        TexDesc.SampleDesc.Count = 1;
        TexDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

        D3D12_HEAP_PROPERTIES HeapProps = {};
        HeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        HRESULT Hr = Device->CreateCommittedResource(
            &HeapProps,
            D3D12_HEAP_FLAG_NONE,
            &TexDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            Win32::UuidOf<ID3D12Resource>(),
            Win32::PpvArgs(m_NoiseTexture.ReleaseAndGetAddressOf())
        );

        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to create SSAO noise texture");
            return;
        }

        // Upload noise data
        const u64 uploadBufferSize = NoiseSize * NoiseSize * sizeof(DirectX::XMFLOAT4);
        D3D12_HEAP_PROPERTIES UploadHeapProps = {};
        UploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC UploadDesc = {};
        UploadDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        UploadDesc.Width = uploadBufferSize;
        UploadDesc.Height = 1;
        UploadDesc.DepthOrArraySize = 1;
        UploadDesc.MipLevels = 1;
        UploadDesc.SampleDesc.Count = 1;
        UploadDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        ComPtr<ID3D12Resource> uploadBuffer;
        Device->CreateCommittedResource(
            &UploadHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &UploadDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            Win32::UuidOf<ID3D12Resource>(),
            Win32::PpvArgs(uploadBuffer.ReleaseAndGetAddressOf())
        );

        // Copy data to upload buffer
        void* mappedData;
        D3D12_RANGE readRange = {0, 0};
        uploadBuffer->Map(0, &readRange, &mappedData);
        memcpy(mappedData, noiseData, uploadBufferSize);
        uploadBuffer->Unmap(0, nullptr);

        // Note: The actual copy and transition should be done via command list
        // For simplicity, we'll need to handle this in the Renderer initialization

        // Create SRV for noise texture
        m_NoiseSRVIndex = SRVHeap->Allocate();
        D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
        SRVDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
        SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        SRVDesc.Texture2D.MipLevels = 1;
        Device->CreateShaderResourceView(m_NoiseTexture.Get(), &SRVDesc,
            SRVHeap->GetCPUHandle(m_NoiseSRVIndex));

        // Store the upload buffer temporarily - Renderer will need to execute the copy
        // For now, we'll handle this in a separate initialization step
    }

    void SSAO::ReleaseTextures() {
        m_SSAOTexture.Reset();
        m_SSAOBlurredTexture.Reset();
    }

    void SSAO::UpdateConstants(
        const DirectX::XMMATRIX& Projection,
        const DirectX::XMMATRIX& InverseProjection,
        u32 ScreenWidth,
        u32 ScreenHeight)
    {
        DirectX::XMStoreFloat4x4(&m_Constants.Projection, Projection);
        DirectX::XMStoreFloat4x4(&m_Constants.InverseProjection, InverseProjection);

        m_Constants.NoiseScale.x = static_cast<float>(ScreenWidth) / 4.0f;  // Noise texture is 4x4
        m_Constants.NoiseScale.y = static_cast<float>(ScreenHeight) / 4.0f;

        m_Constants.Radius = m_Radius;
        m_Constants.Bias = m_Bias;
        m_Constants.Power = m_Power;
        m_Constants.ScreenWidth = ScreenWidth;
        m_Constants.ScreenHeight = ScreenHeight;

        // Copy to GPU
        if (m_ConstantBufferMapped) {
            memcpy(m_ConstantBufferMapped, &m_Constants, sizeof(SSAOConstants));
        }
    }

} // namespace HOX
