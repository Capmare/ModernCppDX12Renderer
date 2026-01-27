//
// Created by capma on 27-Jan-26.
//

module;
#include <d3d12.h>

module HOX.Mesh;

import HOX.Context;
import HOX.Logger;

namespace HOX {


    void Mesh::CreateBuffers(const std::vector<MeshVertex> &Vertices, const std::vector<u32> &Indices) {
        auto& Allocator = GetDeviceContext().m_Allocator;

        m_VertexCount = Vertices.size();
        m_IndexCount = Indices.size();

        // Vertex buffer
        const u64 VertexBufferSize = sizeof(MeshVertex) * m_VertexCount;

        m_VertexBuffer = Allocator->Allocate(VertexBufferSize, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);

        // copy to gpu
        void* MappedData = nullptr;
        D3D12_RANGE Range = {0, 0};
        m_VertexBuffer.Resource->Map(0,&Range,&MappedData);
        std::memcpy(MappedData,Vertices.data(),VertexBufferSize);
        m_VertexBuffer.Resource->Unmap(0,nullptr);

        // Vertex Buffer view
        m_VertexBufferView.BufferLocation = m_VertexBuffer.Resource->GetGPUVirtualAddress();
        m_VertexBufferView.SizeInBytes = static_cast<UINT>(VertexBufferSize);
        m_VertexBufferView.StrideInBytes = sizeof(MeshVertex);

        // index buffer
        const u64 IndexBufferSize = sizeof(u32) * m_IndexCount;

        m_IndexBuffer = Allocator->Allocate(IndexBufferSize, D3D12_HEAP_TYPE_UPLOAD, D3D12_RESOURCE_STATE_GENERIC_READ);

        m_IndexBuffer.Resource->Map(0,&Range,&MappedData);
        std::memcpy(MappedData,Indices.data(),IndexBufferSize);
        m_IndexBuffer.Resource->Unmap(0,nullptr);

        m_IndexBufferView.BufferLocation = m_IndexBuffer.Resource->GetGPUVirtualAddress();
        m_IndexBufferView.SizeInBytes = static_cast<UINT>(IndexBufferSize);
        m_IndexBufferView.Format = DXGI_FORMAT_R32_UINT; // 32bit indices

        Logger::LogMessage(Severity::Info,
            "Mesh created: " + std::to_string(m_VertexCount) + " vertices, " +
            std::to_string(m_IndexCount) + " indices");

        GetDeviceContext().m_Cleaner->AddToCleaner([this]() {
           this->Release();
        });

    }

    void Mesh::Bind(ID3D12GraphicsCommandList *CommandList) const {
        CommandList->IASetVertexBuffers(0,1,&m_VertexBufferView);
        CommandList->IASetIndexBuffer(&m_IndexBufferView);
    }

    void Mesh::Draw(ID3D12GraphicsCommandList *CommandList) const {
        CommandList->DrawIndexedInstanced(m_IndexCount, 1, 0, 0, 0);
    }

    void Mesh::Release() {
        if (m_bReleased) return;
        m_bReleased = true;
        auto& Allocator = GetDeviceContext().m_Allocator;
        if (Allocator) {
            if (m_VertexBuffer.Allocation) { Allocator->FreeAllocation(m_VertexBuffer); }
            if (m_IndexBuffer.Allocation) { Allocator->FreeAllocation(m_IndexBuffer); }
        }

        m_VertexCount = 0;
        m_IndexCount = 0;

    }
}
