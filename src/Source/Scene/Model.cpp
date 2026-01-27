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
    }

    std::size_t Model::GetMeshCount() const {
        return m_Meshes.size();
    }
}
