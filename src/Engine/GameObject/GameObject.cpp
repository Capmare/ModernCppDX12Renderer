//
// Created by capma on 27-Jan-26.
//

module;
#include <d3d12.h>
#include <DirectXMath.h>

module HOX.GameObject;

import std;
import HOX.Types;
import HOX.Model;
import HOX.Transform;
import HOX.MemoryAllocator;
import HOX.Context;
import HOX.Logger;
import HOX.DescriptorHeap;


namespace HOX {

    void GameObject::CreateConstantBuffer() {
        auto& Allocator = GetDeviceContext().m_Allocator;

        m_ConstantBufferAllocation = Allocator->Allocate(PerObjectConstantsAlignedSize,D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);

        if (!m_ConstantBufferAllocation.Resource) {
            Logger::LogMessage(Severity::Error, "Failed to allocate ConstantBuffer resource");
            return;
        }

        D3D12_RANGE Range = {0,0};
        HRESULT Hr = m_ConstantBufferAllocation.Resource->Map(0,&Range, reinterpret_cast<void**>(&m_ConstantBufferMapped));
        if (FAILED(Hr)) {
            Logger::LogMessage(Severity::Error, "Failed to map ConstantBuffer resource");
            m_ConstantBufferMapped = nullptr;
        }

        UpdateConstantBuffer();

        GetDeviceContext().m_Cleaner->AddToCleaner([this]() {
            this->Release();
        });
    }

    void GameObject::UpdateConstantBuffer() const {
        if (!m_ConstantBufferMapped) return;

        DirectX::XMMATRIX WorldMatrix = m_Transform.GetWorldMatrix();

        PerObjectConstants Constants{};
        DirectX::XMStoreFloat4x4(&Constants.m_WorldMatrix,WorldMatrix);

        memcpy(m_ConstantBufferMapped, &Constants, sizeof(PerObjectConstants));

    }

    void GameObject::Draw(ID3D12GraphicsCommandList *CommandList, DescriptorHeap* SRVHeap, u32 DefaultTextureIndex) {
        if (!m_Model || !m_ConstantBufferAllocation.Resource) return;

        CommandList->SetGraphicsRootConstantBufferView(
            RootParams::ObjectCBV,
            m_ConstantBufferAllocation.Resource->GetGPUVirtualAddress());

        m_Model->Draw(CommandList, SRVHeap, DefaultTextureIndex);
    }

    void GameObject::Release() {

        if (m_ConstantBufferAllocation.Resource && m_ConstantBufferMapped) {
            m_ConstantBufferAllocation.Resource->Unmap(0,nullptr);
            m_ConstantBufferMapped = nullptr;
        }

        auto& Allocator = GetDeviceContext().m_Allocator;
        if (m_ConstantBufferAllocation.Allocation && Allocator) {
            Allocator->FreeAllocation(m_ConstantBufferAllocation);
        }

    }
}
