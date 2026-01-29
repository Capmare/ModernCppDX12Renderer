#define TILE_SIZE 16
#define MAX_LIGHTS_PER_TILE 256
#define PI 3.14159265359

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

// Textures
Texture2D g_AlbedoTexture : register(t0);
Texture2D g_NormalMap : register(t1);
Texture2D g_MetallicRoughnessMap : register(t2);  // G = Roughness, B = Metallic (glTF standard)
SamplerState g_Sampler : register(s0);

// Light data
StructuredBuffer<GPULight> Lights : register(t3);
StructuredBuffer<uint2> LightGrid : register(t4);        // Per-tile: (offset, count)
StructuredBuffer<uint> LightIndexList : register(t5);    // Flat list of light indices

// Screen info passed via root constants or another cbuffer
cbuffer ScreenConstants : register(b2) {
    uint2 ScreenDimensions;
    uint2 TileCount;
};

struct PSInput {
    float4 position : SV_POSITION;
    float3 worldPos : WORLDPOS;
    float3 normal : NORMAL;
    float3 tangent : TANGENT;
    float2 texCoord : TEXCOORD0;
    float4 color : COLOR;
};

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

float3 CalculateDirectionalLight(GPULight light, float3 N, float3 V, float3 albedo, float metallic, float roughness) {
    float3 L = -normalize(light.Direction);
    float3 radiance = light.Color * light.Intensity;

    return CookTorranceBRDF(L, V, N, albedo, metallic, roughness) * radiance;
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

    return normalize(mul(tangentNormal, TBN));
}

// ================== Main ==================

float4 main(PSInput input) : SV_Target {
    // Sample textures
    float4 albedoSample = g_AlbedoTexture.Sample(g_Sampler, input.texCoord);
    float3 albedo = albedoSample.rgb * input.color.rgb;

    // Alpha test
    clip(albedoSample.a * input.color.a - 0.5);

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

    // Ambient term (simple, could be replaced with IBL)
    float3 ambient = albedo * 0.03;
    lighting += ambient;

    // Process lights for this tile
    for (uint i = 0; i < lightCount; i++) {
        uint lightIndex = LightIndexList[lightOffset + i];
        GPULight light = Lights[lightIndex];

        if (light.Type == 0) { // Directional
            lighting += CalculateDirectionalLight(light, N, V, albedo, metallic, roughness);
        }
        else if (light.Type == 1) { // Point
            lighting += CalculatePointLight(light, input.worldPos, N, V, albedo, metallic, roughness);
        }
        else if (light.Type == 2) { // Spot
            lighting += CalculateSpotLight(light, input.worldPos, N, V, albedo, metallic, roughness);
        }
    }

    // Output with original alpha
    return float4(lighting, albedoSample.a * input.color.a);
}
