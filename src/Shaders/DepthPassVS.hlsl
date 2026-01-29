cbuffer CameraBuffer : register(b0)
{
    row_major float4x4 ViewProjection;
};

cbuffer ObjectBuffer : register(b1)
{
    row_major float4x4 World;
};

struct VSInput
{
    float3 Position : POSITION;
};

struct VSOutput
{
    float4 Position : SV_Position;
};

VSOutput main(VSInput In)
{
    VSOutput Out;
    float4 worldPos = mul(float4(In.Position, 1.0f), World);
    Out.Position = mul(worldPos, ViewProjection);
    return Out;
}
