// Shadow Map Vertex Shader
// Transforms vertices to light space for shadow map generation
// Passes texture coordinates for alpha testing

// Light view-projection passed as root constants (matches XMFLOAT4X4 row-major storage)
cbuffer LightConstants : register(b0) {
    row_major float4x4 LightViewProjection;
};

cbuffer ObjectConstants : register(b1) {
    row_major float4x4 World;
};

struct VSInput {
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float3 Tangent : TANGENT;
    float2 TexCoord : TEXCOORD0;
    float4 Color : COLOR;
};

struct VSOutput {
    float4 Position : SV_Position;
    float2 TexCoord : TEXCOORD0;
};

VSOutput main(VSInput input) {
    VSOutput output;

    // Transform to world space
    float4 worldPos = mul(float4(input.Position, 1.0), World);

    // Transform to light clip space
    output.Position = mul(worldPos, LightViewProjection);

    // Pass texture coordinates for alpha testing in pixel shader
    output.TexCoord = input.TexCoord;

    return output;
}
