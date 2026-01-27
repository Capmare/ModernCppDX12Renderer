//
// Created by capma on 27-Jan-26.
//

module;
#include <d3d12.h>

export module HOX.LightManager;

import HOX.Types;
import HOX.LightTypes;
import HOX.Win32;
import HOX.MemoryAllocator;
import HOX.DescriptorHeap;

export namespace HOX {
    using HOX::Win32::ComPtr;
    class LightManager {
    public:
        LightManager() = default;
        virtual ~LightManager() = default;

        LightManager(const LightManager&) = delete;
        LightManager& operator=(const LightManager&) = delete;
        LightManager(LightManager&&) noexcept = default;
        LightManager& operator=(LightManager&&) noexcept = default;

        void Initialize(const DescriptorHeap* SRVHeap, u32 InitialCapacity = 1024); // Create Light Buffer
        void Shutdown();
        u32 AddLight(const GPULight& Light);
        void RemoveLight(u32 Index);

        void UpdateGPUBuffer();

        GPULight GetLight(u32 Index);
        u32 GetLightCount() const;
        u32 GetSRVIndex() const;
        D3D12_GPU_VIRTUAL_ADDRESS GetGPUVirtualAddress() const;

        void MarkDirty();

    private:
        std::vector<GPULight> m_Lights{};
        BufferAllocation m_LightBuffer{};
        void* m_MappedData{};
        u32 m_Capacity{};
        u32 m_SRVIndex{};

        bool m_bIsDirty{false};

        bool m_bIsFree{false};

    };
}
