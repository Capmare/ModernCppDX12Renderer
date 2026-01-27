//
// Created by capma on 27-Jan-26.
//

module;
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

module HOX.ModelLoader;

import std;
import HOX.Model;
import HOX.Mesh;
import HOX.Logger;
import HOX.Types;


namespace HOX {
    std::unique_ptr<Model> ModelLoader::LoadFromFile(const std::string &FilePath) {
        Assimp::Importer Importer;

        const aiScene* Scene = Importer.ReadFile(FilePath,
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

        // Extract just the filename for the model name
        std::size_t LastSlash = FilePath.find_last_of("/\\");
        std::string FileName = (LastSlash != std::string::npos)
            ? FilePath.substr(LastSlash + 1)
            : FilePath;
        OutModel->SetName(FileName);

        // Process the node tree starting from root
        ProcessNode(Scene->mRootNode, Scene, *OutModel);

        Logger::LogMessage(Severity::Info,
            std::format("Loaded model '{}' with {} meshes", FileName, OutModel->GetMeshCount()));

        return OutModel;

    }

    void ModelLoader::ProcessNode(const aiNode *Node, const aiScene* Scene, Model &OutModel) {
        for (unsigned int i = 0; i < Node->mNumMeshes; ++i) {
            // Node->mMeshes[i] is an index into Scene->mMeshes
            const aiMesh* AiMesh = Scene->mMeshes[Node->mMeshes[i]];
            auto Mesh = ProcessMesh(AiMesh, *Scene);
            if (Mesh) {
                OutModel.AddMesh(std::move(Mesh));
            }
        }

        for (unsigned int i = 0; i < Node->mNumChildren; ++i) {
            ProcessNode(Node->mChildren[i], Scene, OutModel);
        }
    }

    std::unique_ptr<Mesh> ModelLoader::ProcessMesh(const aiMesh *AiMesh, const aiScene &Scene) {
        std::vector<MeshVertex> Vertices;
        std::vector<u32> Indices;

        // Reserve space for efficiency
        Vertices.reserve(AiMesh->mNumVertices);
        Indices.reserve(AiMesh->mNumFaces * 3);  // Triangulated, so 3 indices per face

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
                Vertex.Normal = {0.0f, 1.0f, 0.0f};  // Default up
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
                Vertex.Color = {1.0f, 1.0f, 1.0f, 1.0f};  // Default white
            }

            Vertices.push_back(Vertex);
        }

        // Process indices (faces)
        for (unsigned int i = 0; i < AiMesh->mNumFaces; ++i) {
            const aiFace& Face = AiMesh->mFaces[i];
            // We triangulated, so each face should have exactly 3 indices
            for (unsigned int j = 0; j < Face.mNumIndices; ++j) {
                Indices.push_back(Face.mIndices[j]);
            }
        }

        // Create our Mesh and upload to GPU
        auto OutMesh = std::make_unique<Mesh>();
        OutMesh->CreateBuffers(Vertices, Indices);

        return OutMesh;
    }
}
