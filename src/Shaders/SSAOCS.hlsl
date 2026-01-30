// SSAO Compute Shader
// Screen-Space Ambient Occlusion using hemisphere sampling

#define KERNEL_SIZE 64
#define TILE_SIZE 8

cbuffer SSAOConstants : register(b0) {
    float4 Samples[KERNEL_SIZE];           // Hemisphere sample kernel
    row_major float4x4 Projection;
    row_major float4x4 InverseProjection;
    float2 NoiseScale;                     // screenSize / noiseTextureSize
    float Radius;
    float Bias;
    float Power;
    uint ScreenWidth;
    uint ScreenHeight;
    float Padding;
};

Texture2D<float> DepthTexture : register(t0);
Texture2D<float4> NoiseTexture : register(t1);
SamplerState NoiseSampler : register(s0);

RWTexture2D<float> SSAOOutput : register(u0);

// Reconstruct view-space position from depth
float3 ReconstructViewPosition(uint2 pixelCoord, float depth) {
    float2 uv = (float2(pixelCoord) + 0.5) / float2(ScreenWidth, ScreenHeight);

    // Convert UV to NDC
    float4 clipPos = float4(uv * 2.0 - 1.0, depth, 1.0);
    clipPos.y = -clipPos.y; // Flip Y for D3D

    // Transform to view space
    float4 viewPos = mul(clipPos, InverseProjection);
    return viewPos.xyz / viewPos.w;
}

// Reconstruct normal from depth buffer using gradients
float3 ReconstructNormalFromDepth(uint2 pixelCoord) {
    float depthC = DepthTexture[pixelCoord];
    float depthL = DepthTexture[pixelCoord - uint2(1, 0)];
    float depthR = DepthTexture[pixelCoord + uint2(1, 0)];
    float depthU = DepthTexture[pixelCoord - uint2(0, 1)];
    float depthD = DepthTexture[pixelCoord + uint2(0, 1)];

    float3 posC = ReconstructViewPosition(pixelCoord, depthC);
    float3 posL = ReconstructViewPosition(pixelCoord - uint2(1, 0), depthL);
    float3 posR = ReconstructViewPosition(pixelCoord + uint2(1, 0), depthR);
    float3 posU = ReconstructViewPosition(pixelCoord - uint2(0, 1), depthU);
    float3 posD = ReconstructViewPosition(pixelCoord + uint2(0, 1), depthD);

    // Use the smaller gradient to avoid edge artifacts
    float3 dx = (abs(posR.z - posC.z) < abs(posL.z - posC.z)) ?
                (posR - posC) : (posC - posL);
    float3 dy = (abs(posD.z - posC.z) < abs(posU.z - posC.z)) ?
                (posD - posC) : (posC - posU);

    return normalize(cross(dx, dy));
}

[numthreads(TILE_SIZE, TILE_SIZE, 1)]
void main(uint3 DTid : SV_DispatchThreadID) {
    if (DTid.x >= ScreenWidth || DTid.y >= ScreenHeight) {
        return;
    }

    // Sample depth
    float depth = DepthTexture[DTid.xy];

    // Skip sky pixels (depth = 1.0)
    if (depth >= 1.0) {
        SSAOOutput[DTid.xy] = 1.0;
        return;
    }

    // Reconstruct position and normal
    float3 viewPos = ReconstructViewPosition(DTid.xy, depth);
    float3 normal = ReconstructNormalFromDepth(DTid.xy);

    // Sample noise texture for random rotation
    float2 noiseUV = float2(DTid.xy) / NoiseScale;
    float3 randomVec = NoiseTexture.SampleLevel(NoiseSampler, noiseUV, 0).xyz;

    // Create TBN matrix (tangent-bitangent-normal)
    // Gram-Schmidt process to create orthonormal basis
    float3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    float3 bitangent = cross(normal, tangent);
    float3x3 TBN = float3x3(tangent, bitangent, normal);

    // Sample hemisphere and accumulate occlusion
    float occlusion = 0.0;

    for (int i = 0; i < KERNEL_SIZE; i++) {
        // Get sample position in tangent space, transform to view space
        float3 sampleVec = mul(Samples[i].xyz, TBN);
        float3 samplePos = viewPos + sampleVec * Radius;

        // Project sample to screen space
        float4 offset = mul(float4(samplePos, 1.0), Projection);
        offset.xy /= offset.w;
        offset.xy = offset.xy * 0.5 + 0.5;
        offset.y = 1.0 - offset.y; // Flip Y

        // Sample depth at projected position
        uint2 sampleCoord = uint2(offset.xy * float2(ScreenWidth, ScreenHeight));
        sampleCoord = clamp(sampleCoord, uint2(0, 0), uint2(ScreenWidth - 1, ScreenHeight - 1));

        float sampleDepth = DepthTexture[sampleCoord];
        float3 sampleViewPos = ReconstructViewPosition(sampleCoord, sampleDepth);

        // Range check - fade out occlusion at distance
        float rangeCheck = smoothstep(0.0, 1.0,
            Radius / max(abs(viewPos.z - sampleViewPos.z), 0.0001));

        // Occlusion test: is the sample occluded?
        // In view space, larger Z means farther from camera
        // If actual geometry is closer than our sample, it occludes
        occlusion += (sampleViewPos.z >= samplePos.z + Bias ? 1.0 : 0.0) * rangeCheck;
    }

    // Normalize and invert (1 = no occlusion, 0 = full occlusion)
    occlusion = 1.0 - (occlusion / float(KERNEL_SIZE));

    // Apply power for contrast
    SSAOOutput[DTid.xy] = pow(saturate(occlusion), Power);
}
