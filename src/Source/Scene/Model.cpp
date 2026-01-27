//
// Created by capma on 27-Jan-26.
//

module HOX.Model;

import std;
import HOX.Mesh;
import HOX.DescriptorHeap;
import HOX.Types;

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

    void Model::Draw(ID3D12GraphicsCommandList *CommandList, DescriptorHeap* SRVHeap, u32 DefaultTextureIndex) const {
        for (std::size_t i = 0; i < m_Meshes.size(); ++i) {
            auto& Mesh = m_Meshes[i];
            // Bind the correct texture for this mesh
            i32 TextureIdx = Mesh->GetTextureIndex();
            if (TextureIdx >= 0 && TextureIdx < static_cast<i32>(m_Textures.size())) {
                // Use mesh's texture
                u32 SrvIndex = m_Textures[TextureIdx]->GetSRVIndex();
                CommandList->SetGraphicsRootDescriptorTable(RootParams::TextureSRV, SRVHeap->GetGPUHandle(SrvIndex));
            } else {
                // Use default texture
                CommandList->SetGraphicsRootDescriptorTable(RootParams::TextureSRV, SRVHeap->GetGPUHandle(DefaultTextureIndex));
            }

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
