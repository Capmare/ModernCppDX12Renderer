//
// Created by capma on 27-Jan-26.
//


module;
#include <d3d12.h>
#include <DirectXMath.h>


export module HOX.Mesh;

import std;
import HOX.Win32;
import HOX.Types;
import HOX.MemoryAllocator;


export namespace HOX {
    using HOX::Win32::ComPtr;

    struct MeshVertex {
        DirectX::XMFLOAT3 Position;
        DirectX::XMFLOAT3 Normal;
        DirectX::XMFLOAT2 TexCoord;
        DirectX::XMFLOAT4 Color;
    };

    class Mesh {
    public:
        Mesh() = default;
        virtual ~Mesh();

        Mesh(const Mesh&) = delete;
        Mesh(Mesh&&) noexcept = delete;
        Mesh& operator=(const Mesh&) = delete;
        Mesh& operator=(Mesh&&) noexcept = delete;

        void CreateBuffers(const std::vector<MeshVertex>& Vertices, const std::vector<u32>& Indices);

        void Bind(ID3D12GraphicsCommandList* CommandList) const;

        void Draw(ID3D12GraphicsCommandList* CommandList) const;

        void Release();

        u32 GetIndexCount() const { return m_IndexCount; };
        u32 GetVertexCount() const { return m_VertexCount; };

    private:
        BufferAllocation m_VertexBuffer{};
        BufferAllocation m_IndexBuffer{};

        D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView{};
        D3D12_INDEX_BUFFER_VIEW m_IndexBufferView{};

        u32 m_VertexCount{0};
        u32 m_IndexCount{0};
    };

}