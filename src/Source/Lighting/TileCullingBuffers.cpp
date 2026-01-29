//
// Created by capma on 28-Jan-26.
//
module;
#include <d3d12.h>


module HOX.TileCullingBuffers;

import HOX.Context;
import HOX.LightTypes;

namespace HOX {
    void TileCullingBuffers::Initialize(u32 ScreenWidth, u32 ScreenHeight, DescriptorHeap *SRVHeap) {
        if (m_bInitialized) return;
        CreateBuffers(ScreenWidth, ScreenHeight, SRVHeap);
        m_bInitialized = true;

    }

    void TileCullingBuffers::Resize(u32 ScreenWidth, u32 ScreenHeight, DescriptorHeap *SRVHeap) {
        ReleaseBuffers();
        CreateBuffers(ScreenWidth, ScreenHeight, SRVHeap);
    }

    void TileCullingBuffers::ShutDown() {
        ReleaseBuffers();
        m_bInitialized = false;
    }

    void TileCullingBuffers::CreateBuffers(u32 ScreenWidth, u32 ScreenHeight, DescriptorHeap *SRVHeap) {

        m_TileCountX = (ScreenWidth + LightConstants::TileSize - 1) / LightConstants::TileSize;
        m_TileCountY = (ScreenHeight + LightConstants::TileSize - 1) / LightConstants::TileSize;
        const u32 TotalTiles = GetTotalTileCount();

        auto &Allocator = GetDeviceContext().m_Allocator;
        auto Device = GetDeviceContext().m_Device.Get();

        // Light grid: 2 uints per tile (offset and count) = bytes per tile
        u32 LightGridSize = TotalTiles * sizeof(u32) * 2;

        u32 LightIndexListSize = TotalTiles * sizeof(u32) * LightConstants::MaxLightsPerTile;





        // Making the UAV buffer
        D3D12_HEAP_PROPERTIES HeapProperties = {};
        HeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // gpu only

        D3D12_RESOURCE_DESC ResourceDesc = {};
        ResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        ResourceDesc.Width = LightGridSize;
        ResourceDesc.Height = 1;
        ResourceDesc.DepthOrArraySize = 1;
        ResourceDesc.MipLevels = 1;
        ResourceDesc.SampleDesc.Count = 1;
        ResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        Device->CreateCommittedResource(
            &HeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &ResourceDesc,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS, // Start in UAV state for compute shader
            nullptr,
            HOX::Win32::UuidOf<ID3D12Resource>(),
            HOX::Win32::PpvArgs(m_LightGridBuffer.Resource.ReleaseAndGetAddressOf()));

        ResourceDesc.Width = LightIndexListSize;

        Device->CreateCommittedResource(
            &HeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &ResourceDesc,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS, // Start in UAV state for compute shader
            nullptr,
            HOX::Win32::UuidOf<ID3D12Resource>(),
            HOX::Win32::PpvArgs(m_LightIndexListBuffer.Resource.ReleaseAndGetAddressOf()));



        // Light Index List UAV
        D3D12_UNORDERED_ACCESS_VIEW_DESC IndexListUAVDesc = {};
        IndexListUAVDesc.Format = DXGI_FORMAT_UNKNOWN;
        IndexListUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        IndexListUAVDesc.Buffer.FirstElement = 0;
        IndexListUAVDesc.Buffer.NumElements = TotalTiles * LightConstants::MaxLightsPerTile;
        IndexListUAVDesc.Buffer.StructureByteStride = sizeof(u32);
        IndexListUAVDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

        m_LightIndexListUAVIndex = SRVHeap->Allocate();
        Device->CreateUnorderedAccessView(
            m_LightIndexListBuffer.Resource.Get(),
            nullptr,
            &IndexListUAVDesc,
            SRVHeap->GetCPUHandle(m_LightIndexListUAVIndex));

        // Light Index List SRV (for pixel shader read)
        D3D12_SHADER_RESOURCE_VIEW_DESC IndexListSRVDesc = {};
        IndexListSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
        IndexListSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        IndexListSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        IndexListSRVDesc.Buffer.FirstElement = 0;
        IndexListSRVDesc.Buffer.NumElements = TotalTiles * LightConstants::MaxLightsPerTile;
        IndexListSRVDesc.Buffer.StructureByteStride = sizeof(u32);
        IndexListSRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

        m_LightIndexListSRVIndex = SRVHeap->Allocate();
        Device->CreateShaderResourceView(
            m_LightIndexListBuffer.Resource.Get(),
            &IndexListSRVDesc,
            SRVHeap->GetCPUHandle(m_LightIndexListSRVIndex));



        // UAV descriptor

        D3D12_UNORDERED_ACCESS_VIEW_DESC UAVDesc = {};
        UAVDesc.Format = DXGI_FORMAT_UNKNOWN;
        UAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        UAVDesc.Buffer.FirstElement = 0;
        UAVDesc.Buffer.NumElements = TotalTiles;
        UAVDesc.Buffer.StructureByteStride = sizeof(u32) * 2;
        UAVDesc.Buffer.CounterOffsetInBytes = 0;
        UAVDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

        m_LightGridUAVIndex = SRVHeap->Allocate();
        Device->CreateUnorderedAccessView(
            m_LightGridBuffer.Resource.Get(),
            nullptr,
            &UAVDesc,
            SRVHeap->GetCPUHandle(m_LightGridUAVIndex));


        // SRV descriptor
        D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
        SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
        SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        SRVDesc.Buffer.FirstElement = 0;
        SRVDesc.Buffer.NumElements = TotalTiles;
        SRVDesc.Buffer.StructureByteStride = sizeof(u32) * 2;
        SRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

        m_LightGridSRVIndex = SRVHeap->Allocate();
        Device->CreateShaderResourceView(
            m_LightGridBuffer.Resource.Get(),
            &SRVDesc,
            SRVHeap->GetCPUHandle(m_LightGridSRVIndex));


        ResourceDesc.Width = sizeof(u32);

        Device->CreateCommittedResource(
            &HeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &ResourceDesc,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS, // Start in UAV state for compute shader
            nullptr,
            HOX::Win32::UuidOf<ID3D12Resource>(),
            HOX::Win32::PpvArgs(m_CounterBuffer.Resource.ReleaseAndGetAddressOf()));

        // Counter UAV - structured buffer with 1 element of uint
        D3D12_UNORDERED_ACCESS_VIEW_DESC CounterUAVDesc = {};
        CounterUAVDesc.Format = DXGI_FORMAT_UNKNOWN;  // Structured buffer
        CounterUAVDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
        CounterUAVDesc.Buffer.FirstElement = 0;
        CounterUAVDesc.Buffer.NumElements = 1;
        CounterUAVDesc.Buffer.StructureByteStride = sizeof(u32);
        CounterUAVDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

        m_CounterUAVIndex = SRVHeap->Allocate();
        Device->CreateUnorderedAccessView(
            m_CounterBuffer.Resource.Get(),
            nullptr,
            &CounterUAVDesc,
            SRVHeap->GetCPUHandle(m_CounterUAVIndex));

        // Create upload buffer with zero value for clearing the counter via copy
        D3D12_HEAP_PROPERTIES UploadHeapProps = {};
        UploadHeapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

        D3D12_RESOURCE_DESC ZeroBufferDesc = {};
        ZeroBufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        ZeroBufferDesc.Width = sizeof(u32);
        ZeroBufferDesc.Height = 1;
        ZeroBufferDesc.DepthOrArraySize = 1;
        ZeroBufferDesc.MipLevels = 1;
        ZeroBufferDesc.SampleDesc.Count = 1;
        ZeroBufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        Device->CreateCommittedResource(
            &UploadHeapProps,
            D3D12_HEAP_FLAG_NONE,
            &ZeroBufferDesc,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            HOX::Win32::UuidOf<ID3D12Resource>(),
            HOX::Win32::PpvArgs(m_CounterZeroBuffer.ReleaseAndGetAddressOf()));

        // Initialize the upload buffer with zero
        void* pData = nullptr;
        D3D12_RANGE ReadRange = {0, 0};
        m_CounterZeroBuffer->Map(0, &ReadRange, &pData);
        u32 Zero = 0;
        memcpy(pData, &Zero, sizeof(u32));
        m_CounterZeroBuffer->Unmap(0, nullptr);

        GetDeviceContext().m_Cleaner->AddToCleaner([this]() {
           this->ShutDown();
        });
    }

    void TileCullingBuffers::ReleaseBuffers() {
        m_LightGridBuffer.Resource.Reset();
        m_LightIndexListBuffer.Resource.Reset();
        m_CounterBuffer.Resource.Reset();
        m_CounterZeroBuffer.Reset();
    }
}
