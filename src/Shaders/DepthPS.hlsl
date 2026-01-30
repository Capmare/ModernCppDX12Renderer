// Depth Pass Pixel Shader
// Handles alpha testing for transparent objects (leaves, fabric, etc.)

Texture2D g_AlbedoTexture : register(t0);
SamplerState g_Sampler : register(s0);

struct PSInput {
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD0;
};

void main(PSInput input) {
    // Sample alpha from albedo texture
    float alpha = g_AlbedoTexture.Sample(g_Sampler, input.TexCoord).a;

    // Alpha test - discard transparent pixels (higher threshold to avoid edge fringe)
    clip(alpha - 0.5);

    // No color output - depth only
}
