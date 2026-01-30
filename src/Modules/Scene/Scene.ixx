//
// Created by capma on 27-Jan-26.
//

module;
#include <d3d12.h>

export module HOX.Scene;

import std;
import HOX.GameObject;
import HOX.DescriptorHeap;
import HOX.Types;
import HOX.Model;

export namespace HOX {
    class Scene {
    public:
        Scene() = default;
        virtual ~Scene() = default;

        Scene(const Scene&) = delete;
        Scene& operator=(const Scene&) = delete;
        Scene(Scene&&) noexcept = default;
        Scene& operator=(Scene&&) noexcept = default;

        void AddGameObject(std::unique_ptr<GameObject> GO) {
            if (GO) {
                m_GameObjects.emplace_back(std::move(GO));
            }
        }

        void Update(float DeltaTime) {
            for (auto& GameObject : m_GameObjects) {
                GameObject->UpdateConstantBuffer();
            }
        }

        void Render(ID3D12GraphicsCommandList* CommandList, DescriptorHeap* SRVHeap, const Model::DefaultTextureIndices& DefaultTextures) {
            for (auto& GameObject : m_GameObjects) {
                GameObject->Draw(CommandList, SRVHeap, DefaultTextures);
            }
        }

        void DrawDepthOnly(ID3D12GraphicsCommandList* CommandList, DescriptorHeap* SRVHeap, u32 DefaultAlbedoIndex) {
            for (auto& GameObject : m_GameObjects) {
                GameObject->DrawDepthOnly(CommandList, SRVHeap, DefaultAlbedoIndex);
            }
        }

        // Draw for shadow map generation with alpha testing
        void DrawShadow(ID3D12GraphicsCommandList* CommandList, DescriptorHeap* SRVHeap, u32 DefaultAlbedoIndex) {
            for (auto& GameObject : m_GameObjects) {
                GameObject->DrawShadow(CommandList, SRVHeap, DefaultAlbedoIndex);
            }
        }

        void Clear() {
            m_GameObjects.clear();
        }

        [[nodiscard]] std::vector<std::unique_ptr<GameObject>>& GetGameObjects() {return m_GameObjects; };
        [[nodiscard]] std::size_t GetNumGameObjects() const { return m_GameObjects.size(); };

    private:
        std::vector<std::unique_ptr<GameObject>> m_GameObjects{};


    };
}