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
        DirectX::XMFLOAT3 Tangent;
        DirectX::XMFLOAT2 TexCoord;
        DirectX::XMFLOAT4 Color;
    };

    // Material texture indices for PBR
    struct MeshMaterial {
        i32 DiffuseIndex{-1};      // Albedo/base color texture
        i32 NormalIndex{-1};       // Normal map
        i32 MetallicRoughnessIndex{-1}; // Metallic (B) + Roughness (G) packed
    };

    class Mesh {
    public:
        Mesh() = default;
        virtual ~Mesh() = default;

        Mesh(const Mesh&) = delete;
        Mesh(Mesh&&) noexcept = delete;
        Mesh& operator=(const Mesh&) = delete;
        Mesh& operator=(Mesh&&) noexcept = delete;

        void CreateBuffers(const std::vector<MeshVertex>& Vertices, const std::vector<u32>& Indices);

        void Bind(ID3D12GraphicsCommandList* CommandList) const;

        void Draw(ID3D12GraphicsCommandList* CommandList) const;

        void Release();

        [[nodiscard]] u32 GetIndexCount() const { return m_IndexCount; };
        [[nodiscard]] u32 GetVertexCount() const { return m_VertexCount; };

        void SetTexture(i32 Index) {m_Material.DiffuseIndex = Index; };
        [[nodiscard]] i32 GetTextureIndex() const { return m_Material.DiffuseIndex; };

        void SetMaterial(const MeshMaterial& Material) { m_Material = Material; }
        [[nodiscard]] const MeshMaterial& GetMaterial() const { return m_Material; }

    private:
        BufferAllocation m_VertexBuffer{};
        BufferAllocation m_IndexBuffer{};

        D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView{};
        D3D12_INDEX_BUFFER_VIEW m_IndexBufferView{};

        u32 m_VertexCount{0};
        u32 m_IndexCount{0};

        MeshMaterial m_Material{};

        bool m_bReleased{false};

    };

}