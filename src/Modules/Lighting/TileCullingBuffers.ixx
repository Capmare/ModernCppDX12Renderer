//
// Created by capma on 28-Jan-26.
//


module;
#include <d3d12.h>

export module HOX.TileCullingBuffers;

import HOX.Types;
import HOX.Win32;
import HOX.MemoryAllocator;
import HOX.DescriptorHeap;
import HOX.LightTypes;


export namespace HOX {

    using HOX::Win32::ComPtr;

    class TileCullingBuffers {
    public:
        TileCullingBuffers() = default;
        virtual ~TileCullingBuffers() = default;

        TileCullingBuffers(const TileCullingBuffers&) = delete;
        TileCullingBuffers& operator=(const TileCullingBuffers&) = delete;
        TileCullingBuffers(TileCullingBuffers&&) noexcept = default;
        TileCullingBuffers& operator=(TileCullingBuffers&&) noexcept = default;

        void Initialize(u32 ScreenWidth, u32 ScreenHeight, DescriptorHeap* SRVHeap);
        void Resize(u32 ScreenWidth, u32 ScreenHeight, DescriptorHeap* SRVHeap);
        void ShutDown();

        [[nodiscard]] u32 GetLightGridUAVIndex() const { return m_LightGridUAVIndex; }
        [[nodiscard]] u32 GetLightIndexListUAVIndex() const { return m_LightIndexListUAVIndex; }
        [[nodiscard]] u32 GetLightGridSRVIndex() const { return m_LightGridSRVIndex; }
        [[nodiscard]] u32 GetLightIndexListSRVIndex() const { return m_LightIndexListSRVIndex; }

        [[nodiscard]] ID3D12Resource* GetLightGridResource() const { return m_LightGridBuffer.Resource.Get(); }
        [[nodiscard]] ID3D12Resource* GetLightIndexListResource() const { return m_LightIndexListBuffer.Resource.Get(); }

        [[nodiscard]] u32 GetXTileCount() const { return m_TileCountX; }
        [[nodiscard]] u32 GetYTileCount() const { return m_TileCountY; }
        [[nodiscard]] constexpr u32 GetTotalTileCount() const { return m_TileCountX * m_TileCountY; }

        [[nodiscard]] u32 GetCounterUAVIndex() const { return m_CounterUAVIndex; }
        [[nodiscard]] ID3D12Resource* GetCounterResource() const { return m_CounterBuffer.Resource.Get(); }
        [[nodiscard]] ID3D12Resource* GetCounterZeroBuffer() const { return m_CounterZeroBuffer.Get(); }

    private:
        void CreateBuffers(u32 ScreenWidth, u32 ScreenHeight, DescriptorHeap* SRVHeap);
        void ReleaseBuffers();

        BufferAllocation m_LightGridBuffer{}; // per tile: offset + count
        BufferAllocation m_LightIndexListBuffer{}; // flat list of light indices

        u32 m_TileCountX{};
        u32 m_TileCountY{};

        // UAV indices (for compute shader write)
        u32 m_LightGridUAVIndex{};
        u32 m_LightIndexListUAVIndex{};

        // SRV indices (for pixel shader read)
        u32 m_LightGridSRVIndex{};
        u32 m_LightIndexListSRVIndex{};

        BufferAllocation m_CounterBuffer{};
        u32 m_CounterUAVIndex{};
        ComPtr<ID3D12Resource> m_CounterZeroBuffer{}; // Upload buffer with zero for clearing counter

        bool m_bInitialized{false};

    };

}
