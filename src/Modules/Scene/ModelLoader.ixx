//
// Created by capma on 27-Jan-26.
//

module;
#include <assimp/scene.h>
#include <assimp/material.h>
#include <d3d12.h>

export module HOX.ModelLoader;

import std;
import HOX.Model;
import HOX.Mesh;
import HOX.Texture;
import HOX.DescriptorHeap;
import HOX.Types;

export namespace HOX {
    class ModelLoader {
    public:
        ModelLoader() = default;

        virtual ~ModelLoader() = default;

        ModelLoader(const ModelLoader &) = delete;

        ModelLoader(ModelLoader &&) noexcept = delete;

        ModelLoader &operator=(const ModelLoader &) = delete;

        ModelLoader &operator=(ModelLoader &&) noexcept = delete;

        static std::unique_ptr<Model> LoadFromFile(const std::string &FilePath, ID3D12GraphicsCommandList *CommandList,
                                                   DescriptorHeap *SRVHeap);

        static i32 LoadMaterialTexture(
            const aiMaterial *Material,
            aiTextureType Type,
            const aiScene *Scene,
            const std::string &Directory,
            Model &OutModel,
            ID3D12GraphicsCommandList *CommandList,
            DescriptorHeap *SRVHeap);

    private:
        static void ProcessNode(const aiNode *Node, const aiScene *Scene, Model &OutModel, const std::string &Directory,
                                ID3D12GraphicsCommandList *
                                CommandList, DescriptorHeap *SRVHeap);

        static std::unique_ptr<Mesh> ProcessMesh(const aiMesh *AiMesh, const aiScene &Scene, i32 TextureIndex);
    };
}
