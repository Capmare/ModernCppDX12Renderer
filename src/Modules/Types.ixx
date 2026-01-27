//
// Created by capma on 26-Jan-26.
//

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
        constexpr u32 CameraCBV = 0;    // b0 - view/projection matrix
        constexpr u32 ObjectCBV = 1;    // b1 - world matrix
        constexpr u32 TextureSRV = 2;   // t0 - diffuse texture
    }
}

