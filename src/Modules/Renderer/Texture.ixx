//
// Created by capma on 27-Jan-26.
//

module;
#include <d3d12.h>

export module HOX.Texture;

import std;
import HOX.Types;
import HOX.Win32;
import HOX.MemoryAllocator;

export namespace HOX {

    using HOX::Win32::ComPtr;

    class Texture {
    public:
        Texture() = default;
        virtual ~Texture() = default;

        Texture(const Texture&) = delete;
        Texture& operator=(const Texture&) = delete;
        Texture(Texture&&) noexcept = default;
        Texture& operator=(Texture&&) noexcept = default;

        bool LoadFromFile(const std::string& FilePath, ID3D12GraphicsCommandList* CommandLists);

        bool LoadFromMemory(const unsigned char* Data, u32 DataSize, ID3D12GraphicsCommandList* CommandLists);

        bool CreateFromPixels(const unsigned char* Pixels, u32 Width, u32 Height, ID3D12GraphicsCommandList* CommandList);

        void Release();

        void CreateSRV(class DescriptorHeap* SRVHeap, DXGI_FORMAT Format = DXGI_FORMAT_R8G8B8A8_UNORM);

        [[nodiscard]] ID3D12Resource* GetResource() const { return m_TextureResource.Get(); }
        [[nodiscard]] u32 GetWidth() const { return m_Width; }
        [[nodiscard]] u32 GetHeight() const { return m_Height; }
        [[nodiscard]] u32 GetSRVIndex() const { return m_SRVIndex; }
        [[nodiscard]] bool IsValid() const { return m_TextureResource != nullptr; }

        void SetSRVIndex(u32 Index) { m_SRVIndex = Index; }

    private:
        ComPtr<ID3D12Resource> m_TextureResource{};
        BufferAllocation m_UploadBuffer{};

        u32 m_Width{};
        u32 m_Height{};
        u32 m_SRVIndex{};

        bool m_bReleased{false};

    };


}
