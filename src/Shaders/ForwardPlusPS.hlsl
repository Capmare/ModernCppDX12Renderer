#define TILE_SIZE 16
#define MAX_LIGHTS_PER_TILE 256
#define PI 3.14159265359
#define NUM_CASCADES 4

// Light structure (must match C++ GPULight)
struct GPULight {
    float3 Position;
    float Range;
    float3 Direction;
    float SpotOuterAngle;
    float3 Color;
    float Intensity;
    uint Type;
    float SpotInnerAngle;
    float2 Padding;
};

// Constants
cbuffer CameraConstants : register(b0) {
    row_major float4x4 ViewProjection;
    float3 CameraPosition;
    float CameraPadding;
};

// Screen info passed via root constants
cbuffer ScreenConstants : register(b2) {
    uint2 ScreenDimensions;
    uint2 TileCount;
};

// Shadow constants
cbuffer ShadowConstants : register(b3) {
    row_major float4x4 CascadeViewProjection[NUM_CASCADES];
    float4 CascadeSplits;  // View-space Z distances for cascade boundaries
    float3 LightDirection;
    float ShadowBias;
    row_major float4x4 CameraViewMatrix;  // For world->view transform
};

// Tone mapping constants
cbuffer ToneMappingConstants : register(b4) {
    float Exposure;
    float3 ToneMappingPadding;
};

// Textures
Texture2D g_AlbedoTexture : register(t0);
Texture2D g_NormalMap : register(t1);
Texture2D g_MetallicRoughnessMap : register(t2);  // G = Roughness, B = Metallic (glTF standard)

// Light data
StructuredBuffer<GPULight> Lights : register(t3);
StructuredBuffer<uint2> LightGrid : register(t4);        // Per-tile: (offset, count)
StructuredBuffer<uint> LightIndexList : register(t5);    // Flat list of light indices

// Shadow and SSAO textures
Texture2DArray<float> ShadowMap : register(t6);  // Cascade shadow map array
Texture2D<float> SSAOTexture : register(t7);     // SSAO occlusion values

// Samplers
SamplerState g_Sampler : register(s0);
SamplerComparisonState ShadowSampler : register(s1);  // PCF comparison sampler

struct PSInput {
    float4 position : SV_POSITION;
    float3 worldPos : WORLDPOS;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 texCoord : TEXCOORD0;
    float4 color : COLOR;
};

// ================== Shadow Functions ==================

// Select which cascade to use based on view-space Z
int SelectCascade(float viewZ) {
    int cascade = 0;
    if (viewZ > CascadeSplits.x) cascade = 1;
    if (viewZ > CascadeSplits.y) cascade = 2;
    if (viewZ > CascadeSplits.z) cascade = 3;
    return min(cascade, NUM_CASCADES - 1);
}

// PCF shadow sampling with 3x3 kernel
float SampleShadowPCF(float3 worldPos, int cascade) {
    // Transform to light clip space
    float4 shadowCoord = mul(float4(worldPos, 1.0), CascadeViewProjection[cascade]);
    shadowCoord.xyz /= shadowCoord.w;

    // Convert from [-1,1] to [0,1] UV space
    // D3D clip space: X [-1,1] -> [0,1], Y [-1,1] -> [0,1] (bottom to top in NDC)
    float2 shadowUV = shadowCoord.xy * 0.5 + 0.5;
    // Don't flip Y - the shadow map was rendered with the same coordinate system

    // Check if outside shadow map
    if (shadowUV.x < 0 || shadowUV.x > 1 || shadowUV.y < 0 || shadowUV.y > 1) {
        return 1.0;  // No shadow outside map
    }

    // Depth in D3D is [0,1] in clip space after w divide
    // Scale bias by cascade - farther cascades cover larger areas, need more bias
    float cascadeBias = ShadowBias * float(cascade + 1);
    float depth = shadowCoord.z - cascadeBias;

    // PCF 3x3 kernel
    float shadow = 0.0;
    float texelSize = 1.0 / 2048.0;  // Shadow map size

    [unroll]
    for (int x = -1; x <= 1; x++) {
        [unroll]
        for (int y = -1; y <= 1; y++) {
            float2 offset = float2(x, y) * texelSize;
            shadow += ShadowMap.SampleCmpLevelZero(
                ShadowSampler,
                float3(shadowUV + offset, cascade),
                depth
            );
        }
    }

    return shadow / 9.0;
}

// Main shadow calculation
float CalculateShadow(float3 worldPos) {
    // For static shadow maps, use cascade 0 which covers the main scene area
    // Could extend to use distance from origin for cascade selection
    float distFromOrigin = length(worldPos.xz);

    int cascade = 0;
    if (distFromOrigin > 150.0) cascade = 1;
    if (distFromOrigin > 400.0) cascade = 2;
    if (distFromOrigin > 1200.0) cascade = 3;

    return SampleShadowPCF(worldPos, cascade);
}

// ================== Tone Mapping ==================

// ACES Filmic tone mapping
float3 ACESFilmic(float3 x) {
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

// ================== PBR Functions ==================

// Normal Distribution Function (GGX/Trowbridge-Reitz)
float DistributionGGX(float3 N, float3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return a2 / max(denom, 0.0001);
}

// Geometry function (Schlick-GGX)
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float denom = NdotV * (1.0 - k) + k;
    return NdotV / max(denom, 0.0001);
}

// Smith's method for geometry obstruction
float GeometrySmith(float3 N, float3 V, float3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

// Fresnel-Schlick approximation
float3 FresnelSchlick(float cosTheta, float3 F0) {
    return F0 + (1.0 - F0) * pow(saturate(1.0 - cosTheta), 5.0);
}

// Cook-Torrance BRDF
float3 CookTorranceBRDF(float3 L, float3 V, float3 N, float3 albedo, float metallic, float roughness) {
    float3 H = normalize(V + L);

    // F0 for dielectrics is ~0.04, for metals it's the albedo
    float3 F0 = float3(0.04, 0.04, 0.04);
    F0 = lerp(F0, albedo, metallic);

    // Cook-Torrance specular BRDF
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    float3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);

    float3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0);
    float3 specular = numerator / max(denominator, 0.001);

    // Energy conservation: specular + diffuse must not exceed 1
    float3 kS = F;
    float3 kD = float3(1.0, 1.0, 1.0) - kS;
    kD *= 1.0 - metallic; // Metals have no diffuse

    float NdotL = max(dot(N, L), 0.0);

    // Diffuse is Lambertian
    float3 diffuse = kD * albedo / PI;

    return (diffuse + specular) * NdotL;
}

// ================== Attenuation ==================

float CalculateAttenuation(float distance, float range) {
    // Smooth falloff that reaches zero at range
    float attenuation = saturate(1.0 - (distance / range));
    return attenuation * attenuation;
}

// ================== Light Calculations ==================

float3 CalculatePointLight(GPULight light, float3 worldPos, float3 N, float3 V, float3 albedo, float metallic, float roughness) {
    float3 L = light.Position - worldPos;
    float distance = length(L);

    if (distance > light.Range) return float3(0, 0, 0);

    L = normalize(L);

    float attenuation = CalculateAttenuation(distance, light.Range);
    float3 radiance = light.Color * light.Intensity * attenuation;

    return CookTorranceBRDF(L, V, N, albedo, metallic, roughness) * radiance;
}

float3 CalculateSpotLight(GPULight light, float3 worldPos, float3 N, float3 V, float3 albedo, float metallic, float roughness) {
    float3 L = light.Position - worldPos;
    float distance = length(L);

    if (distance > light.Range) return float3(0, 0, 0);

    L = normalize(L);

    // Spot cone attenuation
    float cosAngle = dot(-L, normalize(light.Direction));
    float cosOuter = cos(light.SpotOuterAngle);
    float cosInner = cos(light.SpotInnerAngle);
    float spotAttenuation = saturate((cosAngle - cosOuter) / (cosInner - cosOuter));

    float distanceAttenuation = CalculateAttenuation(distance, light.Range);
    float3 radiance = light.Color * light.Intensity * distanceAttenuation * spotAttenuation;

    return CookTorranceBRDF(L, V, N, albedo, metallic, roughness) * radiance;
}

float3 CalculateDirectionalLight(GPULight light, float3 worldPos, float3 N, float3 V, float3 albedo, float metallic, float roughness, float shadow) {
    float3 L = -normalize(light.Direction);
    float3 radiance = light.Color * light.Intensity;

    // Apply shadow factor to directional light
    return CookTorranceBRDF(L, V, N, albedo, metallic, roughness) * radiance * shadow;
}

// ================== Normal Mapping ==================

float3 GetNormalFromMap(float3 normalMapSample, float3 worldNormal, float3 worldTangent) {
    // Reconstruct bitangent
    float3 N = normalize(worldNormal);
    float3 T = normalize(worldTangent - dot(worldTangent, N) * N); // Re-orthogonalize
    float3 B = cross(N, T);

    // Construct TBN matrix
    float3x3 TBN = float3x3(T, B, N);

    // Transform normal from tangent space to world space
    // Normal map is in [0,1], convert to [-1,1]
    float3 tangentNormal = normalMapSample * 2.0 - 1.0;
    tangentNormal.y = -tangentNormal.y;

    return normalize(mul(tangentNormal, TBN));
}

// ================== Main ==================

float4 main(PSInput input) : SV_Target {
    // Sample textures
    float4 albedoSample = g_AlbedoTexture.Sample(g_Sampler, input.texCoord);
    float3 albedo = albedoSample.rgb * input.color.rgb;

    // Alpha test - use just texture alpha (higher threshold to avoid edge fringe)
    clip(albedoSample.a - 0.5);

    // Sample normal map (if available, otherwise use vertex normal)
    float3 normalMapSample = g_NormalMap.Sample(g_Sampler, input.texCoord).rgb;
    float3 N;

    // Check if normal map is valid (not default 0,0,0 or uniform color like 0.5,0.5,1.0 for flat)
    if (length(normalMapSample) > 0.1) {
        N = GetNormalFromMap(normalMapSample, input.normal, input.tangent);
    } else {
        N = normalize(input.normal);
    }

    // Sample metallic-roughness (glTF standard: G = roughness, B = metallic)
    float4 mrSample = g_MetallicRoughnessMap.Sample(g_Sampler, input.texCoord);
    float roughness = mrSample.g;
    float metallic = mrSample.b;

    // Default values if no metallic-roughness map
    if (roughness == 0.0 && metallic == 0.0) {
        roughness = 0.5;
        metallic = 0.0;
    }

    // Clamp roughness to avoid divide by zero
    roughness = max(roughness, 0.04);

    // View direction
    float3 V = normalize(CameraPosition - input.worldPos);

    // Calculate shadow for directional light
    float shadow = CalculateShadow(input.worldPos);

    // Sample SSAO
    float2 screenUV = input.position.xy / float2(ScreenDimensions);
    float ao = SSAOTexture.Sample(g_Sampler, screenUV);
    // TEMP: Keep SSAO disabled until textures are properly cleared
    ao = 1.0;

    // Calculate which tile this pixel belongs to
    uint2 pixelCoord = uint2(input.position.xy);
    uint2 tileCoord = pixelCoord / TILE_SIZE;
    uint tileIndex = tileCoord.y * TileCount.x + tileCoord.x;

    // Get light list for this tile
    uint2 lightInfo = LightGrid[tileIndex];
    uint lightOffset = lightInfo.x;
    uint lightCount = lightInfo.y;

    // Accumulate lighting
    float3 lighting = float3(0, 0, 0);

    // Ambient term modulated by SSAO - increased for better shadow visibility
    float3 ambient = albedo * 0.15 * ao;
    lighting += ambient;

    // Process lights for this tile
    for (uint i = 0; i < lightCount; i++) {
        uint lightIndex = LightIndexList[lightOffset + i];
        GPULight light = Lights[lightIndex];

        if (light.Type == 0) { // Directional - apply shadow
            lighting += CalculateDirectionalLight(light, input.worldPos, N, V, albedo, metallic, roughness, shadow);
        }
        else if (light.Type == 1) { // Point
            lighting += CalculatePointLight(light, input.worldPos, N, V, albedo, metallic, roughness);
        }
        else if (light.Type == 2) { // Spot
            lighting += CalculateSpotLight(light, input.worldPos, N, V, albedo, metallic, roughness);
        }
    }

    // Apply exposure
    lighting *= Exposure;

    // Tone mapping (ACES Filmic)
    lighting = ACESFilmic(lighting);

    // DEBUG: Visualize normals (blue = up +Y, green = +Z, red = +X)
    //return float4(N * 0.5 + 0.5, 1.0);

    // Output with original alpha
    return float4(lighting, albedoSample.a * input.color.a);
}
