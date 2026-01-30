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

    void Model::Draw(ID3D12GraphicsCommandList *CommandList, DescriptorHeap* SRVHeap, const DefaultTextureIndices& DefaultTextures) const {
        for (std::size_t i = 0; i < m_Meshes.size(); ++i) {
            auto& Mesh = m_Meshes[i];
            const auto& Material = Mesh->GetMaterial();

            // Bind albedo texture (t0)
            if (Material.DiffuseIndex >= 0 && Material.DiffuseIndex < static_cast<i32>(m_Textures.size())) {
                u32 SrvIndex = m_Textures[Material.DiffuseIndex]->GetSRVIndex();
                CommandList->SetGraphicsRootDescriptorTable(RootParams::TextureSRV, SRVHeap->GetGPUHandle(SrvIndex));
            } else {
                CommandList->SetGraphicsRootDescriptorTable(RootParams::TextureSRV, SRVHeap->GetGPUHandle(DefaultTextures.Albedo));
            }

            // Bind normal map (t1)
            if (Material.NormalIndex >= 0 && Material.NormalIndex < static_cast<i32>(m_Textures.size())) {
                u32 SrvIndex = m_Textures[Material.NormalIndex]->GetSRVIndex();
                CommandList->SetGraphicsRootDescriptorTable(RootParams::NormalMapSRV, SRVHeap->GetGPUHandle(SrvIndex));
            } else {
                CommandList->SetGraphicsRootDescriptorTable(RootParams::NormalMapSRV, SRVHeap->GetGPUHandle(DefaultTextures.NormalMap));
            }

            // Bind metallic-roughness map (t2)
            if (Material.MetallicRoughnessIndex >= 0 && Material.MetallicRoughnessIndex < static_cast<i32>(m_Textures.size())) {
                u32 SrvIndex = m_Textures[Material.MetallicRoughnessIndex]->GetSRVIndex();
                CommandList->SetGraphicsRootDescriptorTable(RootParams::MetallicRoughnessSRV, SRVHeap->GetGPUHandle(SrvIndex));
            } else {
                CommandList->SetGraphicsRootDescriptorTable(RootParams::MetallicRoughnessSRV, SRVHeap->GetGPUHandle(DefaultTextures.MetallicRoughness));
            }

            Mesh->Bind(CommandList);
            Mesh->Draw(CommandList);
        }
    }

    void Model::DrawDepthOnly(ID3D12GraphicsCommandList *CommandList, DescriptorHeap* SRVHeap, u32 DefaultAlbedoIndex) const {
        for (std::size_t i = 0; i < m_Meshes.size(); ++i) {
            auto& Mesh = m_Meshes[i];
            const auto& Material = Mesh->GetMaterial();

            // Bind albedo texture for alpha testing (uses main root signature TextureSRV slot)
            if (Material.DiffuseIndex >= 0 && Material.DiffuseIndex < static_cast<i32>(m_Textures.size())) {
                u32 SrvIndex = m_Textures[Material.DiffuseIndex]->GetSRVIndex();
                CommandList->SetGraphicsRootDescriptorTable(RootParams::TextureSRV, SRVHeap->GetGPUHandle(SrvIndex));
            } else {
                CommandList->SetGraphicsRootDescriptorTable(RootParams::TextureSRV, SRVHeap->GetGPUHandle(DefaultAlbedoIndex));
            }

            Mesh->Bind(CommandList);
            Mesh->Draw(CommandList);
        }
    }

    void Model::DrawShadow(ID3D12GraphicsCommandList *CommandList, DescriptorHeap* SRVHeap, u32 DefaultAlbedoIndex) const {
        for (std::size_t i = 0; i < m_Meshes.size(); ++i) {
            auto& Mesh = m_Meshes[i];
            const auto& Material = Mesh->GetMaterial();

            // Bind albedo texture for alpha testing (root param 2 in shadow root signature)
            if (Material.DiffuseIndex >= 0 && Material.DiffuseIndex < static_cast<i32>(m_Textures.size())) {
                u32 SrvIndex = m_Textures[Material.DiffuseIndex]->GetSRVIndex();
                CommandList->SetGraphicsRootDescriptorTable(2, SRVHeap->GetGPUHandle(SrvIndex));
            } else {
                CommandList->SetGraphicsRootDescriptorTable(2, SRVHeap->GetGPUHandle(DefaultAlbedoIndex));
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
