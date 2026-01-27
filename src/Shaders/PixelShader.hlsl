struct PSInput
{
    float4 position : SV_POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
    float4 color    : COLOR;
};

float4 main(PSInput input) : SV_Target
{
    return input.color;
}
