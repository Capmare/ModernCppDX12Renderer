//
// Created by capma on 27-Jan-26.
//

module;
#include <d3d12.h>


module HOX.LightManager;

import HOX.DescriptorHeap;
import HOX.Types;
import HOX.Context;
import HOX.Logger;

namespace HOX {
    void LightManager::Initialize(const DescriptorHeap* SRVHeap, u32 InitialCapacity) {
        m_Capacity = InitialCapacity;
        // Allocate light buffer
        m_LightBuffer = GetDeviceContext().m_Allocator->Allocate(sizeof(GPULight) * m_Capacity,
            D3D12_HEAP_TYPE_UPLOAD,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            D3D12_RESOURCE_FLAG_NONE
        );

        // Mapping the data to the GPU
        D3D12_RANGE ReadRange = {0,0};
        m_LightBuffer.Resource->Map(0,&ReadRange,&m_MappedData);

        // Get slot in the descriptor heap
        m_SRVIndex = SRVHeap->Allocate();

        // Describe the view
        D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
        SRVDesc.Format = DXGI_FORMAT_UNKNOWN;
        SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        SRVDesc.Buffer.FirstElement = 0;
        SRVDesc.Buffer.NumElements = m_Capacity;
        SRVDesc.Buffer.StructureByteStride = sizeof(GPULight);
        SRVDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

        // Create View
        GetDeviceContext().m_Device->CreateShaderResourceView(
            m_LightBuffer.Resource.Get(),
            &SRVDesc,
            SRVHeap->GetCPUHandle(m_SRVIndex)
        );

        GetDeviceContext().m_Cleaner->AddToCleaner([this]() {
            this->Shutdown();
        });

    }

    void LightManager::Shutdown() {
        if (m_bIsFree) return;
        m_bIsFree = true;
        if (m_MappedData) {
            m_LightBuffer.Resource->Unmap(0, nullptr);
            m_MappedData = nullptr;
        }

        GetDeviceContext().m_Allocator->FreeAllocation(m_LightBuffer);
    }

    u32 LightManager::AddLight(const GPULight &Light) {
        if (m_Lights.size() >= m_Capacity) {
            Logger::LogMessage(Severity::ErrorNoCrash, "LightBuffer full");
            return UINT32_MAX;
        }

        m_Lights.emplace_back(std::move(Light));
        m_bIsDirty = true;
        return m_Lights.size() - 1;
    }

    // swap last so its faster to remove
    void LightManager::RemoveLight(u32 Index) {

        if (Index >= m_Lights.size()) { return; }

        if (Index != m_Lights.size() - 1) {
            std::swap(m_Lights[Index], m_Lights.back());
        }

        m_Lights.pop_back();
        m_bIsDirty = true;
    }

    void LightManager::UpdateGPUBuffer() {
        if (!m_bIsDirty) return;

        if (!m_Lights.empty()) {
            memcpy(m_MappedData, m_Lights.data(), sizeof(GPULight) * m_Lights.size());

        }
        m_bIsDirty = false;

    }

    GPULight LightManager::GetLight(u32 Index) {
        return m_Lights[Index];
    }

    u32 LightManager::GetLightCount() const {
        return m_Lights.size();
    }

    u32 LightManager::GetSRVIndex() const {
        return m_SRVIndex;
    }

    D3D12_GPU_VIRTUAL_ADDRESS LightManager::GetGPUVirtualAddress() const {
        return m_LightBuffer.Resource->GetGPUVirtualAddress();

    }

    void LightManager::MarkDirty() {
        m_bIsDirty = true;
    }
}
