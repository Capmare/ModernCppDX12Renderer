//
// Created by capma on 27-Jan-26.
//

module;
#include <d3d12.h>

export module HOX.Model;

import std;
import HOX.Mesh;
import HOX.Texture;
import HOX.DescriptorHeap;
import HOX.Types;

export namespace HOX {
    class Model {
    public:
        Model() = default;
        virtual ~Model() = default;

        Model(const Model&) = delete;
        Model(Model&&) noexcept = delete;
        // Allow moving
        Model& operator=(const Model&) = default;
        Model& operator=(Model&&) noexcept = default;

        void AddMesh(std::unique_ptr<Mesh> Mesh);
        void AddTexture(std::unique_ptr<Texture> Texture);


        void Draw(ID3D12GraphicsCommandList *CommandList, DescriptorHeap* SRVHeap, u32 DefaultTextureIndex) const;

        void Release();

        [[nodiscard]] std::size_t GetMeshCount() const;
        [[nodiscard]] Texture* GetTexture(std::size_t Index) const;
        [[nodiscard]] std::size_t GetTextureCount() const;

        void SetName(const std::string& Name) { m_Name = Name; };
    private:
        std::vector<std::unique_ptr<Mesh>> m_Meshes{};
        std::vector<std::unique_ptr<Texture>> m_Textures{};
        std::string m_Name{};

    };
}
