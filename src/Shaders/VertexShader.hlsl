cbuffer CameraConstants : register(b0) {
    row_major float4x4 ViewProjection;
    float3 CameraPosition;
    float Padding;
};

cbuffer ObjectConstants: register(b1) {
    row_major float4x4 World;
}

struct VSInput {
    float3 position : POSITION;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 texCoord : TEXCOORD0;
    float4 color : COLOR;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float3 worldPos : WORLDPOS;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 texCoord : TEXCOORD0;
    float4 color : COLOR;
};

VSOutput main(VSInput input) {
    VSOutput output;

    float4 worldPos = mul(float4(input.position, 1.0f), World);
    output.worldPos = worldPos.xyz;
    output.position = mul(worldPos, ViewProjection);

    // Transform normal and tangent to world space (normalize to handle non-uniform scaling)
    output.normal = normalize(mul(input.normal, (float3x3)World));
    output.tangent = normalize(mul(input.tangent, (float3x3)World));
    output.texCoord = input.texCoord;
    output.color = input.color;

    return output;
}
