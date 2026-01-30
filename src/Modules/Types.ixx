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
        // CSM, SSAO, Tone Mapping
        constexpr u32 ShadowMapSRV = 9;       // t6 - shadow map cascade array
        constexpr u32 SSAOSRV = 10;           // t7 - SSAO texture
        constexpr u32 ShadowCBV = 11;         // b3 - shadow matrices and params
        constexpr u32 ToneMappingConstants = 12; // b4 - exposure and tone mapping params
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

    // CSM constants - cascade view-projection matrices and split distances
    constexpr u32 CSM_NUM_CASCADES = 4;
    constexpr u32 CSM_SHADOW_MAP_SIZE = 2048;

    struct ShadowConstants {
        DirectX::XMFLOAT4X4 CascadeViewProjection[CSM_NUM_CASCADES]; // 256 bytes
        DirectX::XMFLOAT4 CascadeSplits;      // x,y,z,w = split distances in view space
        DirectX::XMFLOAT3 LightDirection;
        float ShadowBias;
        DirectX::XMFLOAT4X4 ViewMatrix;       // Camera view matrix for world->view transform
    };

    // Tone mapping constants
    struct ToneMappingConstants {
        float Exposure;
        float Padding[3];
    };

    // SSAO constants
    constexpr u32 SSAO_KERNEL_SIZE = 64;

    struct SSAOConstants {
        DirectX::XMFLOAT4 Samples[SSAO_KERNEL_SIZE]; // Hemisphere sample kernel
        DirectX::XMFLOAT4X4 Projection;
        DirectX::XMFLOAT4X4 InverseProjection;
        DirectX::XMFLOAT2 NoiseScale;         // screenSize / noiseTextureSize
        float Radius;
        float Bias;
        float Power;
        u32 ScreenWidth;
        u32 ScreenHeight;
        float Padding;
    };
}

