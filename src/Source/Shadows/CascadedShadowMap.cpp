//
// Cascaded Shadow Map implementation
//

module;
#include <d3d12.h>
#include <DirectXMath.h>
#include <algorithm>
#include <cmath>

module HOX.CascadedShadowMap;

import HOX.Context;
import HOX.Win32;
import HOX.Logger;

namespace HOX {

    void CascadedShadowMap::Initialize(DescriptorHeap* SRVHeap) {
        if (m_bInitialized) return;

        auto Device = GetDeviceContext().m_Device.Get();

        // Create shadow map texture array (Texture2DArray with 4 slices)
        D3D12_RESOURCE_DESC TexDesc = {};
        TexDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        TexDesc.Width = CSM_SHADOW_MAP_SIZE;
        TexDesc.Height = CSM_SHADOW_MAP_SIZE;
        TexDesc.DepthOrArraySize = CSM_NUM_CASCADES;
        TexDesc.MipLevels = 1;
        TexDesc.Format = DXGI_FORMAT_R32_TYPELESS; // Typeless for both DSV and SRV
        TexDesc.SampleDesc.Count = 1;
        TexDesc.SampleDesc.Quality = 0;
        TexDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        TexDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

        D3D12_HEAP_PROPERTIES HeapProps = {};
        HeapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_CLEAR_VALUE ClearValue = {};
        ClearValue.Format = DXGI_FORMAT_D32_FLOAT;
        ClearValue.DepthStencil.Depth = 1.0f;
        ClearValue.DepthStencil.Stencil = 0;

        HRESULT Hr = Device->CreateCommittedResource(
            &HeapProps,
            D3D12_HEAP_FLAG_NONE,
            &TexDesc,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &ClearValue,
            Win32::UuidOf<ID3D12Resource>(),
            Win32::PpvArgs(m_ShadowMapArray.ReleaseAndGetAddressOf())
        );

        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to create shadow map texture array");
            return;
        }

        // Create DSV heap for 4 cascades
        D3D12_DESCRIPTOR_HEAP_DESC DSVHeapDesc = {};
        DSVHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        DSVHeapDesc.NumDescriptors = CSM_NUM_CASCADES;
        DSVHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

        Hr = Device->CreateDescriptorHeap(
            &DSVHeapDesc,
            Win32::UuidOf<ID3D12DescriptorHeap>(),
            Win32::PpvArgs(m_DSVHeap.ReleaseAndGetAddressOf())
        );

        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to create shadow map DSV heap");
            return;
        }

        m_DSVDescriptorSize = Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

        // Create DSV for each cascade (array slice)
        D3D12_CPU_DESCRIPTOR_HANDLE DSVHandle = m_DSVHeap->GetCPUDescriptorHandleForHeapStart();
        for (u32 i = 0; i < CSM_NUM_CASCADES; i++) {
            D3D12_DEPTH_STENCIL_VIEW_DESC DSVDesc = {};
            DSVDesc.Format = DXGI_FORMAT_D32_FLOAT;
            DSVDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
            DSVDesc.Texture2DArray.MipSlice = 0;
            DSVDesc.Texture2DArray.FirstArraySlice = i;
            DSVDesc.Texture2DArray.ArraySize = 1;

            Device->CreateDepthStencilView(m_ShadowMapArray.Get(), &DSVDesc, DSVHandle);
            DSVHandle.ptr += m_DSVDescriptorSize;
        }

        // Create SRV for sampling in pixel shader (Texture2DArray)
        m_SRVIndex = SRVHeap->Allocate();

        D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
        SRVDesc.Format = DXGI_FORMAT_R32_FLOAT;
        SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
        SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        SRVDesc.Texture2DArray.MostDetailedMip = 0;
        SRVDesc.Texture2DArray.MipLevels = 1;
        SRVDesc.Texture2DArray.FirstArraySlice = 0;
        SRVDesc.Texture2DArray.ArraySize = CSM_NUM_CASCADES;

        Device->CreateShaderResourceView(
            m_ShadowMapArray.Get(),
            &SRVDesc,
            SRVHeap->GetCPUHandle(m_SRVIndex)
        );

        // Create constant buffer for shadow data
        D3D12_HEAP_PROPERTIES UploadHeapProps = {};
        UploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC CBDesc = {};
        CBDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        // Round up to 256-byte alignment
        CBDesc.Width = (sizeof(ShadowConstants) + 255) & ~255;
        CBDesc.Height = 1;
        CBDesc.DepthOrArraySize = 1;
        CBDesc.MipLevels = 1;
        CBDesc.SampleDesc.Count = 1;
        CBDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        Hr = Device->CreateCommittedResource(
            &UploadHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &CBDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            Win32::UuidOf<ID3D12Resource>(),
            Win32::PpvArgs(m_ConstantBuffer.ReleaseAndGetAddressOf())
        );

        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to create shadow constant buffer");
            return;
        }

        D3D12_RANGE ReadRange = {0, 0};
        m_ConstantBuffer->Map(0, &ReadRange, &m_ConstantBufferMapped);

        m_bInitialized = true;
        Logger::LogMessage(Severity::Info, "CascadedShadowMap initialized");
    }

    void CascadedShadowMap::Shutdown() {
        if (m_ConstantBufferMapped) {
            m_ConstantBuffer->Unmap(0, nullptr);
            m_ConstantBufferMapped = nullptr;
        }
        m_ShadowMapArray.Reset();
        m_DSVHeap.Reset();
        m_ConstantBuffer.Reset();
        m_bInitialized = false;
    }

    void CascadedShadowMap::Update(
        const DirectX::XMMATRIX& CameraView,
        const DirectX::XMMATRIX& CameraProjection,
        const DirectX::XMFLOAT3& LightDirection,
        float ZNear,
        float ZFar)
    {
        // Limit shadow distance
        float ShadowFar = std::min(ZFar, m_MaxShadowDistance);

        CalculateCascadeSplits(ZNear, ShadowFar);
        CalculateCascadeMatrices(CameraView, CameraProjection, LightDirection, ZNear, ShadowFar);
    }

    void CascadedShadowMap::CalculateCascadeSplits(float ZNear, float ZFar) {
        // Practical split scheme - blend between logarithmic and uniform
        m_CascadeSplits[0] = ZNear;

        for (u32 i = 1; i <= CSM_NUM_CASCADES; i++) {
            float p = static_cast<float>(i) / CSM_NUM_CASCADES;

            // Logarithmic split
            float logSplit = ZNear * std::pow(ZFar / ZNear, p);

            // Uniform split
            float uniformSplit = ZNear + (ZFar - ZNear) * p;

            // Blend between them
            m_CascadeSplits[i] = m_CascadeSplitLambda * logSplit +
                                 (1.0f - m_CascadeSplitLambda) * uniformSplit;
        }
    }

    void CascadedShadowMap::GetFrustumCornersWorldSpace(
        const DirectX::XMMATRIX& CameraView,
        const DirectX::XMMATRIX& CameraProjection,
        float NearZ,
        float FarZ,
        DirectX::XMVECTOR* OutCorners)
    {
        using namespace DirectX;

        // Get inverse view-projection
        XMMATRIX InvView = XMMatrixInverse(nullptr, CameraView);
        XMMATRIX InvProj = XMMatrixInverse(nullptr, CameraProjection);

        // NDC corners at near and far planes
        // Near plane corners (z = 0 in D3D NDC)
        XMVECTOR ndcCorners[8] = {
            // Near plane
            XMVectorSet(-1.0f, -1.0f, 0.0f, 1.0f),
            XMVectorSet( 1.0f, -1.0f, 0.0f, 1.0f),
            XMVectorSet( 1.0f,  1.0f, 0.0f, 1.0f),
            XMVectorSet(-1.0f,  1.0f, 0.0f, 1.0f),
            // Far plane
            XMVectorSet(-1.0f, -1.0f, 1.0f, 1.0f),
            XMVectorSet( 1.0f, -1.0f, 1.0f, 1.0f),
            XMVectorSet( 1.0f,  1.0f, 1.0f, 1.0f),
            XMVectorSet(-1.0f,  1.0f, 1.0f, 1.0f),
        };

        // Transform to view space
        XMVECTOR viewCorners[8];
        for (int i = 0; i < 8; i++) {
            XMVECTOR corner = XMVector4Transform(ndcCorners[i], InvProj);
            corner = XMVectorDivide(corner, XMVectorSplatW(corner));
            viewCorners[i] = corner;
        }

        // Get the original near/far from projection
        // For a perspective projection: P[2][2] = zFar/(zFar-zNear), P[3][2] = -zNear*zFar/(zFar-zNear)
        // We need to interpolate between near and far planes
        float viewNearZ = XMVectorGetZ(viewCorners[0]);
        float viewFarZ = XMVectorGetZ(viewCorners[4]);

        // Calculate interpolation factors for our custom near/far
        float nearT = (NearZ - viewNearZ) / (viewFarZ - viewNearZ);
        float farT = (FarZ - viewNearZ) / (viewFarZ - viewNearZ);

        // Interpolate corners for our cascade slice
        for (int i = 0; i < 4; i++) {
            // Near corners
            OutCorners[i] = XMVectorLerp(viewCorners[i], viewCorners[i + 4], nearT);
            // Far corners
            OutCorners[i + 4] = XMVectorLerp(viewCorners[i], viewCorners[i + 4], farT);
        }

        // Transform from view space to world space
        for (int i = 0; i < 8; i++) {
            OutCorners[i] = XMVector3TransformCoord(OutCorners[i], InvView);
        }
    }

    void CascadedShadowMap::CalculateCascadeMatrices(
        const DirectX::XMMATRIX& CameraView,
        const DirectX::XMMATRIX& CameraProjection,
        const DirectX::XMFLOAT3& LightDirection,
        float ZNear,
        float ZFar)
    {
        using namespace DirectX;

        XMVECTOR LightDir = XMVector3Normalize(XMLoadFloat3(&LightDirection));

        // SIMPLE STATIC SHADOW MAP - Fixed position, covers entire scene
        // Use fixed cascade sizes centered at world origin
        float cascadeSizes[CSM_NUM_CASCADES] = { 200.0f, 500.0f, 1500.0f, 5000.0f };

        for (u32 cascade = 0; cascade < CSM_NUM_CASCADES; cascade++) {
            float size = cascadeSizes[cascade];

            // Center at world origin (0, 0, 0) - static, doesn't follow camera
            XMVECTOR center = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);

            // Light position: offset from center along negative light direction
            XMVECTOR lightPos = XMVectorSubtract(center, XMVectorScale(LightDir, size));

            // Up vector - handle light pointing straight down
            XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
            if (std::abs(XMVectorGetY(LightDir)) > 0.99f) {
                up = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
            }

            // Create light view matrix
            XMMATRIX lightView = XMMatrixLookAtLH(lightPos, center, up);

            // Orthographic projection covering the cascade area
            XMMATRIX lightProj = XMMatrixOrthographicLH(
                size * 2.0f,  // width
                size * 2.0f,  // height
                1.0f,         // near
                size * 3.0f   // far
            );

            // Store final view-projection matrix
            XMStoreFloat4x4(&m_CascadeData[cascade].ViewProjection,
                XMMatrixMultiply(lightView, lightProj));
            m_CascadeData[cascade].SplitDistance = m_CascadeSplits[cascade + 1];
        }
    }

    D3D12_CPU_DESCRIPTOR_HANDLE CascadedShadowMap::GetDSVHandle(u32 CascadeIndex) const {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = m_DSVHeap->GetCPUDescriptorHandleForHeapStart();
        handle.ptr += CascadeIndex * m_DSVDescriptorSize;
        return handle;
    }

    DirectX::XMFLOAT4 CascadedShadowMap::GetCascadeSplits() const {
        return DirectX::XMFLOAT4(
            m_CascadeSplits[1],
            m_CascadeSplits[2],
            m_CascadeSplits[3],
            m_CascadeSplits[4]
        );
    }

    void CascadedShadowMap::UpdateConstantBuffer(const DirectX::XMMATRIX& CameraView) {
        // Copy cascade matrices
        for (u32 i = 0; i < CSM_NUM_CASCADES; i++) {
            m_ShadowConstants.CascadeViewProjection[i] = m_CascadeData[i].ViewProjection;
        }

        // Store split distances
        m_ShadowConstants.CascadeSplits = GetCascadeSplits();

        // Store light direction (should be set by caller)
        m_ShadowConstants.ShadowBias = m_ShadowBias;

        // Store camera view matrix for world->view transform in shader
        DirectX::XMStoreFloat4x4(&m_ShadowConstants.ViewMatrix, CameraView);

        // Copy to GPU
        if (m_ConstantBufferMapped) {
            memcpy(m_ConstantBufferMapped, &m_ShadowConstants, sizeof(ShadowConstants));
        }
    }

} // namespace HOX
