// Add at the top of VertexShader.hlsl
cbuffer CameraConstants : register(b0) {
    row_major float4x4 viewProjection;
};

struct VSInput {
    float3 position : POSITION;
    float4 color : COLOR;
};

struct VSOutput {
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

VSOutput main(VSInput input) {
    VSOutput output;

    // Transform position by view-projection matrix
    output.position = mul(float4(input.position, 1.0f), viewProjection);

    output.color = input.color;
    return output;
}
