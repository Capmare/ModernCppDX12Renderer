//
// Created by capma on 27-Jan-26.
//

module;
#include <DirectXMath.h>


export module HOX.LightTypes;

import HOX.Types;

export namespace HOX {
    enum class LightType : u32 {
        Directional,
        Point,
        Spot,
    };

    struct alignas(16) GPULight {
        DirectX::XMFLOAT3 m_Position;
        float Range{};
        DirectX::XMFLOAT3 m_Direction;
        float m_SpotOuterAngle{};
        DirectX::XMFLOAT3 m_Color;
        float m_Intensity{};

        LightType m_Type;
        float m_SpotInnerAngle{};
        float Padding[2];
    };

    static_assert(sizeof(GPULight) == 64, "GPULight must be 64 bytes long");

    namespace LightConstants {

        constexpr u32 TileSize = 16;
        constexpr u32 MaxLightsPerTile = 256;

    }


}
