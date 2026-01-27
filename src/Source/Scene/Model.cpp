//
// Created by capma on 27-Jan-26.
//

module HOX.Model;

import std;
import HOX.Mesh;

namespace HOX {
    void Model::AddMesh(std::unique_ptr<Mesh> Mesh) {
        if (Mesh) {
            m_Meshes.emplace_back(std::move(Mesh));
        }
    }

    void Model::AddTexture(std::unique_ptr<Texture> Texture) {
        if (Texture) {
            m_Textures.emplace_back(std::move(Texture));
        }
    }

    void Model::Draw(ID3D12GraphicsCommandList *CommandList) const {
        for (auto& Mesh : m_Meshes) {
            Mesh->Bind(CommandList);
            Mesh->Draw(CommandList);
        }
    }

    void Model::Release() {
        for (auto& Mesh : m_Meshes) {
            Mesh->Release();
        }
        m_Meshes.clear();

        for (auto& Texture : m_Textures) {
            Texture->Release();
        }
        m_Textures.clear();
    }

    std::size_t Model::GetMeshCount() const {
        return m_Meshes.size();
    }

    Texture * Model::GetTexture(std::size_t Index) const {
        if (Index < m_Textures.size()) {
            return m_Textures[Index].get();
        }
        return nullptr;
    }

    std::size_t Model::GetTextureCount() const {
        return m_Textures.size();
    }
}
