// Add at the top of VertexShader.hlsl
cbuffer CameraConstants : register(b0) {
    row_major float4x4 viewProjection;
};

cbuffer ObjectConstants: register(b1) {
    row_major float4x4 world;
}

struct VSInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
    float4 color : COLOR;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
    float4 color : COLOR;
};

VSOutput main(VSInput input) {
    VSOutput output;

    float4 worldPos = mul(float4(input.position, 1.0f), world);
    output.position = mul(worldPos, viewProjection);

    output.normal = mul(input.normal, (float3x3)world);
    output.texCoord = input.texCoord;
    output.color = input.color;

    return output;
}
