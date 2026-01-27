//
// Created by capma on 27-Jan-26.
//

module;
#include <assimp/scene.h>

export module HOX.ModelLoader;

import std;
import HOX.Model;
import HOX.Mesh;

export namespace HOX {
    class ModelLoader {
    public:
        ModelLoader() = default;
        virtual ~ModelLoader() = default;

        ModelLoader(const ModelLoader&) = delete;
        ModelLoader(ModelLoader&&) noexcept = delete;
        ModelLoader& operator=(const ModelLoader&) = delete;
        ModelLoader& operator=(ModelLoader&&) noexcept = delete;

        static std::unique_ptr<Model> LoadFromFile(const std::string& FilePath);

    private:
        static void ProcessNode(const aiNode *Node, const aiScene *Scene, Model &OutModel);

        static std::unique_ptr<Mesh> ProcessMesh(const aiMesh* Mesh, const aiScene& Scene);
    };
}
