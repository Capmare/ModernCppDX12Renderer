//
// Created by capma on 27-Jan-26.
//

module;
#include <d3d12.h>
#include "stb_image.h"

module HOX.Texture;

import std;
import HOX.Types;
import HOX.Win32;
import HOX.MemoryAllocator;
import HOX.Context;
import HOX.Logger;

namespace HOX {


    bool Texture::LoadFromFile(const std::string &FilePath, ID3D12GraphicsCommandList *CommandLists) {
        int Width{};
        int Height{};
        int Channels{};

        unsigned char* Pixels{stbi_load(FilePath.c_str(), &Width, &Height, &Channels, 4)};

        if (!Pixels) {
            Logger::LogMessage(Severity::Error, std::format("Failed to load texture '{}' '{}'", FilePath, stbi_failure_reason()));
            return false;
        }

        bool Result = CreateFromPixels(Pixels, Width, Height, CommandLists);
        stbi_image_free(Pixels);

        if (Result) {
            Logger::LogMessage(Severity::Info, std::format("Texture Created Succesfully '{} ({}x{})'",FilePath, Width, Height ));
        }
        return Result;
    }

    bool Texture::LoadFromMemory(const unsigned char *Data, u32 DataSize, ID3D12GraphicsCommandList *CommandLists) {
        int Width{};
        int Height{};
        int Channels{};

        unsigned char* Pixels{stbi_load_from_memory(Data,static_cast<int>(DataSize),&Width,&Height,&Channels,4)};
        if (!Pixels) {
            Logger::LogMessage(Severity::Error, std::format("Failed to load texture from memory: {}", stbi_failure_reason()));
            return false;
        }

        bool Result = CreateFromPixels(Pixels, Width, Height, CommandLists);
        stbi_image_free(Pixels);
        return Result;
    }

    bool Texture::CreateFromPixels(const unsigned char *Pixels, u32 Width, u32 Height,
                                   ID3D12GraphicsCommandList *CommandList) {

        m_Width = Width;
        m_Height = Height;

        auto& Device = GetDeviceContext().m_Device;
        auto& Allocator = GetDeviceContext().m_Allocator;

        D3D12_RESOURCE_DESC TextureDesc{};
        TextureDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        TextureDesc.Alignment = 0;
        TextureDesc.Width = Width;
        TextureDesc.Height = Height;
        TextureDesc.DepthOrArraySize = 1;
        TextureDesc.MipLevels = 1;
        TextureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 4 bytes per ixel rgba
        TextureDesc.SampleDesc.Count = 1;
        TextureDesc.SampleDesc.Quality = 0;
        TextureDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        TextureDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

        D3D12_HEAP_PROPERTIES HeapProperties{};
        HeapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // gpu only

        HRESULT Hr = Device->CreateCommittedResource(
            &HeapProperties,
            D3D12_HEAP_FLAG_NONE,
            &TextureDesc,
            D3D12_RESOURCE_STATE_COPY_DEST, // start in copy dest state
            nullptr,
            HOX::Win32::UuidOf<ID3D12Resource>(),
            HOX::Win32::PpvArgs(m_TextureResource.GetAddressOf())
            );

        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to create texture resource");
            return false;
        }

        // Calculate upload buffer size (must account for row pitch alignment)
        const u64 BytesPerPixel = 4;  // RGBA
        const u64 RowPitch = (Width * BytesPerPixel + D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1)
                           & ~(D3D12_TEXTURE_DATA_PITCH_ALIGNMENT - 1);
        const u64 UploadBufferSize = RowPitch * Height;

        // Create upload buffer
        m_UploadBuffer = Allocator->Allocate(
            UploadBufferSize,
            D3D12_HEAP_TYPE_UPLOAD,
            D3D12_RESOURCE_STATE_GENERIC_READ
        );

        if (!m_UploadBuffer.Resource) {
            Logger::LogMessage(Severity::Error, "Failed to create texture upload buffer");
            return false;
        }

        // Copy pixels to upload buffer
        void* MappedData = nullptr;
        D3D12_RANGE ReadRange{0, 0};
        Hr = m_UploadBuffer.Resource->Map(0, &ReadRange, &MappedData);

        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to map texture upload buffer");
            return false;
        }

        const u64 SrcRowPitch = Width * BytesPerPixel;
        for (u32 Row = 0; Row < Height; ++Row) {
            memcpy(
                static_cast<unsigned char*>(MappedData) + Row * RowPitch,
                Pixels + Row * SrcRowPitch,
                SrcRowPitch
            );
        }

        m_UploadBuffer.Resource->Unmap(0, nullptr);

        // Copy from upload buffer to texture
        D3D12_TEXTURE_COPY_LOCATION SrcLocation{};
        SrcLocation.pResource = m_UploadBuffer.Resource.Get();
        SrcLocation.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        SrcLocation.PlacedFootprint.Offset = 0;
        SrcLocation.PlacedFootprint.Footprint.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        SrcLocation.PlacedFootprint.Footprint.Width = Width;
        SrcLocation.PlacedFootprint.Footprint.Height = Height;
        SrcLocation.PlacedFootprint.Footprint.Depth = 1;
        SrcLocation.PlacedFootprint.Footprint.RowPitch = static_cast<UINT>(RowPitch);

        D3D12_TEXTURE_COPY_LOCATION DstLocation{};
        DstLocation.pResource = m_TextureResource.Get();
        DstLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        DstLocation.SubresourceIndex = 0;

        CommandList->CopyTextureRegion(&DstLocation, 0, 0, 0, &SrcLocation, nullptr);

        // Transition texture to shader resource state
        D3D12_RESOURCE_BARRIER Barrier{};
        Barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        Barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        Barrier.Transition.pResource = m_TextureResource.Get();
        Barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        Barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        Barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

        CommandList->ResourceBarrier(1, &Barrier);

        GetDeviceContext().m_Cleaner->AddToCleaner([this]() {
            this->Release();
        });

        return true;
    }

    void Texture::Release() {
        if (m_bReleased) return;
        m_bReleased = true;
        if (GetDeviceContext().m_Allocator) {
            auto& Allocator = GetDeviceContext().m_Allocator;
            if (m_UploadBuffer.Resource) {
                Allocator->FreeAllocation(m_UploadBuffer);
            }
        }
        m_UploadBuffer = {};

        m_TextureResource.Reset();
        m_Width = 0;
        m_Height = 0;
    }
}
