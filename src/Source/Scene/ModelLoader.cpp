//
// Created by capma on 27-Jan-26.
//

module;
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <d3d12.h>

module HOX.ModelLoader;

import std;
import HOX.Model;
import HOX.Mesh;
import HOX.Logger;
import HOX.Types;
import HOX.Context;

namespace HOX {
    std::unique_ptr<Model> ModelLoader::LoadFromFile(
        const std::string &FilePath,
        ID3D12GraphicsCommandList *CommandList,
        DescriptorHeap *SRVHeap) {
        Assimp::Importer Importer;

        const aiScene *Scene = Importer.ReadFile(FilePath,
                                                 aiProcess_Triangulate |
                                                 aiProcess_GenNormals |
                                                 aiProcess_FlipUVs |
                                                 aiProcess_CalcTangentSpace
        );

        if (!Scene || (Scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !Scene->mRootNode) {
            Logger::LogMessage(Severity::Error,
                               std::format("Assimp error loading '{}': {}", FilePath, Importer.GetErrorString()));
            return nullptr;
        }

        auto OutModel = std::make_unique<Model>();

        // Extract directory from file path (for loading textures)
        std::string Directory = "";
        std::size_t LastSlash = FilePath.find_last_of("/\\");
        if (LastSlash != std::string::npos) {
            Directory = FilePath.substr(0, LastSlash + 1);
        }

        // Extract just the filename for the model name
        std::string FileName = (LastSlash != std::string::npos)
                                   ? FilePath.substr(LastSlash + 1)
                                   : FilePath;
        OutModel->SetName(FileName);

        // Process the node tree starting from root
        ProcessNode(Scene->mRootNode, Scene, *OutModel, Directory, CommandList, SRVHeap);

        Logger::LogMessage(Severity::Info,
                           std::format("Loaded model '{}' with {} meshes, {} textures",
                                       FileName, OutModel->GetMeshCount(), OutModel->GetTextureCount()));

        return OutModel;
    }


    i32 ModelLoader::LoadMaterialTexture(const aiMaterial *Material, aiTextureType Type, const aiScene *Scene,
                                         const std::string &Directory, Model &OutModel,
                                         ID3D12GraphicsCommandList *CommandList,
                                         DescriptorHeap *SRVHeap) {
          // Check if material has this texture type
    if (Material->GetTextureCount(Type) == 0) {
        return -1;  // No texture, use default
    }

    aiString TexturePath;
    Material->GetTexture(Type, 0, &TexturePath);

    auto NewTexture = std::make_unique<Texture>();
    bool Loaded = false;

    // Check if it's an embedded texture (path starts with '*')
    if (TexturePath.C_Str()[0] == '*') {
        // Embedded texture - index follows the '*'
        int EmbeddedIndex = std::atoi(TexturePath.C_Str() + 1);
        if (EmbeddedIndex >= 0 && static_cast<unsigned>(EmbeddedIndex) < Scene->mNumTextures) {
            const aiTexture* EmbeddedTex = Scene->mTextures[EmbeddedIndex];

            if (EmbeddedTex->mHeight == 0) {
                // Compressed format (PNG/JPG stored as raw bytes)
                // mWidth contains the size in bytes
                Loaded = NewTexture->LoadFromMemory(
                    reinterpret_cast<const unsigned char*>(EmbeddedTex->pcData),
                    EmbeddedTex->mWidth,
                    CommandList);
            } else {
                // Uncompressed RGBA data
                Loaded = NewTexture->CreateFromPixels(
                    reinterpret_cast<const unsigned char*>(EmbeddedTex->pcData),
                    EmbeddedTex->mWidth,
                    EmbeddedTex->mHeight,
                    CommandList);
            }
        }
    } else {
        // External texture file
        std::string FullPath = Directory + TexturePath.C_Str();
        Loaded = NewTexture->LoadFromFile(FullPath, CommandList);
    }

    if (!Loaded || !NewTexture->IsValid()) {
        Logger::LogMessage(Severity::Warning,
            std::format("Failed to load texture: {}", TexturePath.C_Str()));
        return -1;
    }

    // Create SRV for this texture
    u32 SrvIndex = SRVHeap->Allocate();
    NewTexture->SetSRVIndex(SrvIndex);

    D3D12_SHADER_RESOURCE_VIEW_DESC SrvDesc{};
    SrvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    SrvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    SrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    SrvDesc.Texture2D.MipLevels = 1;

    GetDeviceContext().m_Device->CreateShaderResourceView(
        NewTexture->GetResource(),
        &SrvDesc,
        SRVHeap->GetCPUHandle(SrvIndex));

    // Add texture to model and return its index
    i32 TextureIndexInModel = static_cast<i32>(OutModel.GetTextureCount());
    OutModel.AddTexture(std::move(NewTexture));

    Logger::LogMessage(Severity::Info,
        std::format("Loaded texture: {} (SRV index: {})", TexturePath.C_Str(), SrvIndex));

    return TextureIndexInModel;
    }


    void ModelLoader::ProcessNode(
        const aiNode *Node,
        const aiScene *Scene,
        Model &OutModel,
        const std::string &Directory,
        ID3D12GraphicsCommandList *CommandList,
        DescriptorHeap *SRVHeap) {
        for (unsigned int i = 0; i < Node->mNumMeshes; ++i) {
            const aiMesh *AiMesh = Scene->mMeshes[Node->mMeshes[i]];

            // Load texture for this mesh's material
            i32 TextureIndex = -1;
            if (AiMesh->mMaterialIndex < Scene->mNumMaterials) {
                const aiMaterial *Material = Scene->mMaterials[AiMesh->mMaterialIndex];
                TextureIndex = LoadMaterialTexture(Material, aiTextureType_DIFFUSE,
                                                   Scene, Directory, OutModel,
                                                   CommandList, SRVHeap);
            }

            auto Mesh = ProcessMesh(AiMesh, *Scene, TextureIndex);
            if (Mesh) {
                OutModel.AddMesh(std::move(Mesh));
            }
        }

        for (unsigned int i = 0; i < Node->mNumChildren; ++i) {
            ProcessNode(Node->mChildren[i], Scene, OutModel, Directory, CommandList, SRVHeap);
        }
    }


    std::unique_ptr<Mesh> ModelLoader::ProcessMesh(const aiMesh *AiMesh, const aiScene &Scene, i32 TextureIndex) {
        std::vector<MeshVertex> Vertices;
        std::vector<u32> Indices;

        // Reserve space for efficiency
        Vertices.reserve(AiMesh->mNumVertices);
        Indices.reserve(AiMesh->mNumFaces * 3); // Triangulated, so 3 indices per face

        // Process vertices
        for (unsigned int i = 0; i < AiMesh->mNumVertices; ++i) {
            MeshVertex Vertex{};

            // Position (always present)
            Vertex.Position.x = AiMesh->mVertices[i].x;
            Vertex.Position.y = AiMesh->mVertices[i].y;
            Vertex.Position.z = AiMesh->mVertices[i].z;

            // Normal (we requested GenNormals, so should always exist)
            if (AiMesh->HasNormals()) {
                Vertex.Normal.x = AiMesh->mNormals[i].x;
                Vertex.Normal.y = AiMesh->mNormals[i].y;
                Vertex.Normal.z = AiMesh->mNormals[i].z;
            } else {
                Vertex.Normal = {0.0f, 1.0f, 0.0f}; // Default up
            }

            // Texture coordinates (first UV channel only)
            if (AiMesh->HasTextureCoords(0)) {
                Vertex.TexCoord.x = AiMesh->mTextureCoords[0][i].x;
                Vertex.TexCoord.y = AiMesh->mTextureCoords[0][i].y;
            } else {
                Vertex.TexCoord = {0.0f, 0.0f};
            }

            // Vertex colors (first color channel)
            if (AiMesh->HasVertexColors(0)) {
                Vertex.Color.x = AiMesh->mColors[0][i].r;
                Vertex.Color.y = AiMesh->mColors[0][i].g;
                Vertex.Color.z = AiMesh->mColors[0][i].b;
                Vertex.Color.w = AiMesh->mColors[0][i].a;
            } else {
                Vertex.Color = {1.0f, 1.0f, 1.0f, 1.0f}; // Default white
            }

            Vertices.push_back(Vertex);
        }

        // Process indices (faces)
        for (unsigned int i = 0; i < AiMesh->mNumFaces; ++i) {
            const aiFace &Face = AiMesh->mFaces[i];
            // We triangulated, so each face should have exactly 3 indices
            for (unsigned int j = 0; j < Face.mNumIndices; ++j) {
                Indices.push_back(Face.mIndices[j]);
            }
        }

        // Create our Mesh and upload to GPU
        auto OutMesh = std::make_unique<Mesh>();
        OutMesh->CreateBuffers(Vertices, Indices);
        OutMesh->SetTexture(TextureIndex);

        return OutMesh;
    }


}
