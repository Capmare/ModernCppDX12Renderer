Texture2D g_Texture : register(t0);
SamplerState g_Sampler : register(s0);

struct PSInput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
    float4 color    : COLOR;
};

float4 main(PSInput input) : SV_Target
{
    float4 texColor = g_Texture.Sample(g_Sampler, input.texCoord);
    return input.color * texColor;
}
