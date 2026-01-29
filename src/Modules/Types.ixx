//
// Created by capma on 26-Jan-26.
//


module;
#include <DirectXMath.h>


export module HOX.Types;
import std;

export namespace HOX {



    using u8  = std::uint8_t;
    using u16 = std::uint16_t;
    using u32 = std::uint32_t;
    using u64 = std::uint64_t;

    using i8  = std::int8_t;
    using i16 = std::int16_t;
    using i32 = std::int32_t;
    using i64 = std::int64_t;

    namespace RootParams {
        constexpr u32 CameraCBV = 0;          // b0 - view/projection matrix + camera position
        constexpr u32 ObjectCBV = 1;          // b1 - world matrix
        constexpr u32 TextureSRV = 2;         // t0 - diffuse/albedo texture
        constexpr u32 NormalMapSRV = 3;       // t1 - normal map
        constexpr u32 MetallicRoughnessSRV = 4; // t2 - metallic-roughness packed texture
        constexpr u32 LightsSRV = 5;          // t3 - lights structured buffer
        constexpr u32 LightGridSRV = 6;       // t4 - per-tile light grid
        constexpr u32 LightIndexListSRV = 7;  // t5 - light index list
        constexpr u32 ScreenConstants = 8;    // b2 - screen dimensions and tile count
    }

    struct ScreenConstants {
        u32 ScreenWidth;
        u32 ScreenHeight;
        u32 TileCountX;
        u32 TileCountY;
    };

    struct CullingConstants {
        DirectX::XMFLOAT4X4 View;
        DirectX::XMFLOAT4X4 Projection;
        DirectX::XMFLOAT4X4 InverseProjection;
        u32 ScreenWidth;
        u32 ScreenHeight;
        u32 TileCountX;
        u32 TileCountY;
        u32 LightCount;
        float Padding[3];
    };
}

