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
                                                 aiProcess_CalcTangentSpace);

        if (!Scene || (Scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !Scene->mRootNode) {
            Logger::LogMessage(Severity::Error,
                               std::format("Assimp error loading '{}': {}", FilePath, Importer.GetErrorString()));
            return nullptr;
        }

        auto OutModel = std::make_unique<Model>();

        std::string Directory{};
        std::size_t LastSlash = FilePath.find_last_of("/\\");
        if (LastSlash != std::string::npos) Directory = FilePath.substr(0, LastSlash + 1);

        std::string FileName = (LastSlash != std::string::npos) ? FilePath.substr(LastSlash + 1) : FilePath;
        OutModel->SetName(FileName);

        std::unordered_map<std::string, i32> textureCache; // key -> textureIndexInModel
        constexpr i32 kUnresolved = -2;
        std::vector<i32> materialDiffuseCache(Scene->mNumMaterials, kUnresolved);
        // matIndex -> textureIndexInModel (or -1)

        ProcessNode(Scene->mRootNode, Scene, *OutModel, Directory, CommandList, SRVHeap,
                    textureCache, materialDiffuseCache);

        Logger::LogMessage(Severity::Info,
                           std::format("Loaded model '{}' with {} meshes, {} textures",
                                       FileName, OutModel->GetMeshCount(), OutModel->GetTextureCount()));

        return OutModel;
    }


    static std::string MakeTextureKey(const aiString &texturePath, const std::string &directory) {
        const char *p = texturePath.C_Str();
        if (!p || !p[0]) return {};

        // Embedded
        if (p[0] == '*') return std::string("embedded:") + p;

        // External: normalize-ish key (cheap)
        std::string key = directory + p;

        // Optional (helps on Windows): lower-case to avoid duplicates by casing
        for (char &c: key) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

        return key;
    }

    i32 ModelLoader::LoadMaterialTexture(
        const aiMaterial *Material,
        aiTextureType Type,
        const aiScene *Scene,
        const std::string &Directory,
        Model &OutModel,
        ID3D12GraphicsCommandList *CommandList,
        DescriptorHeap *SRVHeap,
        std::unordered_map<std::string, i32> &textureCache) {
        if (Material->GetTextureCount(Type) == 0) return -1;

        aiString texturePath;
        Material->GetTexture(Type, 0, &texturePath);

        const std::string key = MakeTextureKey(texturePath, Directory);
        if (!key.empty()) {
            if (auto it = textureCache.find(key); it != textureCache.end())
                return it->second; // already loaded
        }

        auto newTex = std::make_unique<Texture>();
        bool loaded = false;

        const char *cstr = texturePath.C_Str();
        if (cstr[0] == '*') {
            int embeddedIndex = std::atoi(cstr + 1);
            if (embeddedIndex >= 0 && static_cast<unsigned>(embeddedIndex) < Scene->mNumTextures) {
                const aiTexture *embedded = Scene->mTextures[embeddedIndex];

                if (embedded->mHeight == 0) {
                    loaded = newTex->LoadFromMemory(
                        reinterpret_cast<const unsigned char *>(embedded->pcData),
                        embedded->mWidth,
                        CommandList);
                } else {
                    loaded = newTex->CreateFromPixels(
                        reinterpret_cast<const unsigned char *>(embedded->pcData),
                        embedded->mWidth,
                        embedded->mHeight,
                        CommandList);
                }
            }
        } else {
            std::string fullPath = Directory + cstr;
            loaded = newTex->LoadFromFile(fullPath, CommandList);
        }

        if (!loaded || !newTex->IsValid()) {
            Logger::LogMessage(Severity::Warning,
                               std::format("Failed to load texture: {}", texturePath.C_Str()));
            return -1;
        }

        newTex->CreateSRV(SRVHeap);

        i32 textureIndexInModel = static_cast<i32>(OutModel.GetTextureCount());
        OutModel.AddTexture(std::move(newTex));

        if (!key.empty())
            textureCache.emplace(key, textureIndexInModel);

        return textureIndexInModel;
    }


    void ModelLoader::ProcessNode(
        const aiNode *Node,
        const aiScene *Scene,
        Model &OutModel,
        const std::string &Directory,
        ID3D12GraphicsCommandList *CommandList,
        DescriptorHeap *SRVHeap,
        std::unordered_map<std::string, i32> &textureCache,
        std::vector<i32> &materialDiffuseCache) {
        constexpr i32 kUnresolved = -2;

        for (unsigned i = 0; i < Node->mNumMeshes; ++i) {
            const aiMesh *AiMesh = Scene->mMeshes[Node->mMeshes[i]];

            i32 textureIndex = -1;

            if (AiMesh->mMaterialIndex < Scene->mNumMaterials) {
                i32 &cached = materialDiffuseCache[AiMesh->mMaterialIndex];
                if (cached == kUnresolved) {
                    const aiMaterial *mat = Scene->mMaterials[AiMesh->mMaterialIndex];
                    cached = LoadMaterialTexture(mat, aiTextureType_DIFFUSE, Scene,
                                                 Directory, OutModel, CommandList, SRVHeap,
                                                 textureCache);
                }
                textureIndex = cached; // -1 if none/failed
            }

            auto mesh = ProcessMesh(AiMesh, *Scene, textureIndex);
            if (mesh) OutModel.AddMesh(std::move(mesh));
        }

        for (unsigned i = 0; i < Node->mNumChildren; ++i) {
            ProcessNode(Node->mChildren[i], Scene, OutModel, Directory, CommandList, SRVHeap,
                        textureCache, materialDiffuseCache);
        }
    }


    std::unique_ptr<Mesh> ModelLoader::ProcessMesh(const aiMesh *AiMesh, const aiScene &Scene, i32 TextureIndex) {
        (void) Scene;

        const std::size_t vCount = static_cast<std::size_t>(AiMesh->mNumVertices);
        const std::size_t fCount = static_cast<std::size_t>(AiMesh->mNumFaces);

        const bool useParallel = (vCount >= 50'000) || (fCount >= 50'000);

        const bool hasNormals = AiMesh->HasNormals();
        const bool hasUV0 = AiMesh->HasTextureCoords(0);
        const bool hasCol0 = AiMesh->HasVertexColors(0);

        std::vector<MeshVertex> vertices(vCount);

        std::vector<std::size_t> vid(vCount);
        std::iota(vid.begin(), vid.end(), 0);

        auto buildVertex = [&](std::size_t i) {
            MeshVertex v{};

            const aiVector3D &p = AiMesh->mVertices[i];
            v.Position = {p.x, p.y, p.z};

            if (hasNormals) {
                const aiVector3D &n = AiMesh->mNormals[i];
                v.Normal = {n.x, n.y, n.z};
            } else {
                v.Normal = {0.0f, 1.0f, 0.0f};
            }

            if (hasUV0) {
                const aiVector3D &t = AiMesh->mTextureCoords[0][i];
                v.TexCoord = {t.x, t.y};
            } else {
                v.TexCoord = {0.0f, 0.0f};
            }

            if (hasCol0) {
                const aiColor4D &c = AiMesh->mColors[0][i];
                v.Color = {c.r, c.g, c.b, c.a};
            } else {
                v.Color = {1.0f, 1.0f, 1.0f, 1.0f};
            }

            vertices[i] = v;
        };

        if (useParallel) std::for_each(std::execution::par_unseq, vid.begin(), vid.end(), buildVertex);
        else std::for_each(std::execution::seq, vid.begin(), vid.end(), buildVertex);

        // Indices (fast triangulated path)
        std::vector<u32> indices;
        indices.resize(fCount * 3);

        std::vector<std::size_t> fid(fCount);
        std::iota(fid.begin(), fid.end(), 0);

        auto buildFace = [&](std::size_t fi) {
            const aiFace &face = AiMesh->mFaces[fi];
            const std::size_t base = fi * 3;

            // assuming triangulated
            indices[base + 0] = static_cast<u32>(face.mIndices[0]);
            indices[base + 1] = static_cast<u32>(face.mIndices[1]);
            indices[base + 2] = static_cast<u32>(face.mIndices[2]);
        };

        if (useParallel) std::for_each(std::execution::par_unseq, fid.begin(), fid.end(), buildFace);
        else std::for_each(std::execution::seq, fid.begin(), fid.end(), buildFace);

        auto outMesh = std::make_unique<Mesh>();
        outMesh->CreateBuffers(vertices, indices);
        outMesh->SetTexture(TextureIndex);
        return outMesh;
    }
}
