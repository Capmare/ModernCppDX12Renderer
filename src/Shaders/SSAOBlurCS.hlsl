// SSAO Blur Compute Shader
// Simple box blur to smooth SSAO output

#define BLUR_SIZE 4
#define TILE_SIZE 8

cbuffer BlurConstants : register(b0) {
    uint ScreenWidth;
    uint ScreenHeight;
    uint2 Padding;
};

Texture2D<float> SSAOInput : register(t0);
RWTexture2D<float> SSAOOutput : register(u0);

[numthreads(TILE_SIZE, TILE_SIZE, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
    if (DTid.x >= ScreenWidth || DTid.y >= ScreenHeight) {
        return;
    }

    float result = 0.0;
    int count = 0;

    // Box blur
    for (int x = -BLUR_SIZE; x <= BLUR_SIZE; x++) {
        for (int y = -BLUR_SIZE; y <= BLUR_SIZE; y++) {
            int2 coord = int2(DTid.xy) + int2(x, y);

            // Clamp to screen bounds
            coord = clamp(coord, int2(0, 0), int2(ScreenWidth - 1, ScreenHeight - 1));

            result += SSAOInput[coord];
            count++;
        }
    }

    SSAOOutput[DTid.xy] = result / float(count);
}
