#define TILE_SIZE 16
#define MAX_LIGHTS_PER_TILE 256

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
cbuffer CullingConstants : register(b0) {
    row_major float4x4 View;
    row_major float4x4 Projection;
    row_major float4x4 InverseProjection;
    uint ScreenWidth;
    uint ScreenHeight;
    uint TileCountX;
    uint TileCountY;
    uint LightCount;
    float3 Padding;
};

// Input buffers
StructuredBuffer<GPULight> Lights : register(t0);
Texture2D<float> DepthTexture : register(t1);

// Output buffers (UAV)
RWStructuredBuffer<uint2> LightGrid : register(u0);      // Per-tile: (offset, count)
RWStructuredBuffer<uint> LightIndexList : register(u1);  // Flat list of light indices

// Global atomic counter for light index list
globallycoherent RWStructuredBuffer<uint> LightIndexCounter : register(u2);

// Shared memory for the tile
groupshared uint TileMinDepthInt;
groupshared uint TileMaxDepthInt;
groupshared uint TileLightCount;
groupshared uint TileLightIndices[MAX_LIGHTS_PER_TILE];
groupshared float4 TileFrustumPlanes[4]; // Left, Right, Top, Bottom planes in view space

// Convert depth buffer value to linear view-space Z (positive, in front of camera)
float DepthToLinearZ(float depth) {
    // For D3D standard projection:
    // P[2][2] = zFar / (zFar - zNear)
    // P[3][2] = -zNear * zFar / (zFar - zNear)
    // depth = (z * P[2][2] + P[3][2]) / z = P[2][2] + P[3][2] / z
    // Solving for z: z = P[3][2] / (depth - P[2][2])
    float P22 = Projection[2][2];
    float P32 = Projection[3][2];
    return P32 / (depth - P22);
}

[numthreads(TILE_SIZE, TILE_SIZE, 1)]
void main(
    uint3 GroupId : SV_GroupID,
    uint3 GroupThreadId : SV_GroupThreadID,
    uint GroupIndex : SV_GroupIndex,
    uint3 DispatchThreadId : SV_DispatchThreadID
) {
    // Initialize shared memory (first thread only)
    if (GroupIndex == 0) {
        TileMinDepthInt = 0xFFFFFFFF;
        TileMaxDepthInt = 0;
        TileLightCount = 0;

        // Calculate tile frustum planes in view space
        float2 tileMin = float2(GroupId.xy) / float2(TileCountX, TileCountY);
        float2 tileMax = float2(GroupId.xy + 1) / float2(TileCountX, TileCountY);

        // Convert to NDC [-1, 1]
        tileMin = tileMin * 2.0f - 1.0f;
        tileMax = tileMax * 2.0f - 1.0f;
        tileMin.y = -tileMin.y; // Flip Y for D3D
        tileMax.y = -tileMax.y;

        // Swap Y since we flipped
        float tempY = tileMin.y;
        tileMin.y = tileMax.y;
        tileMax.y = tempY;

        // Calculate frustum planes (pointing inward)
        // Left plane: normal points right (+X)
        TileFrustumPlanes[0] = float4(1, 0, -tileMin.x / Projection[0][0], 0);
        // Right plane: normal points left (-X)
        TileFrustumPlanes[1] = float4(-1, 0, tileMax.x / Projection[0][0], 0);
        // Bottom plane: normal points up (+Y)
        TileFrustumPlanes[2] = float4(0, 1, -tileMin.y / Projection[1][1], 0);
        // Top plane: normal points down (-Y)
        TileFrustumPlanes[3] = float4(0, -1, tileMax.y / Projection[1][1], 0);

        // Normalize planes
        for (int p = 0; p < 4; p++) {
            TileFrustumPlanes[p] /= length(TileFrustumPlanes[p].xyz);
        }
    }
    GroupMemoryBarrierWithGroupSync();

    // Get pixel coordinates
    uint2 PixelCoord = DispatchThreadId.xy;

    // Sample depth buffer (if within screen bounds)
    float depth = 1.0f;
    if (PixelCoord.x < ScreenWidth && PixelCoord.y < ScreenHeight) {
        depth = DepthTexture[PixelCoord];
    }

    // Convert to uint for atomic min/max
    uint depthInt = asuint(depth);

    // Find min/max depth in tile using atomics
    InterlockedMin(TileMinDepthInt, depthInt);
    InterlockedMax(TileMaxDepthInt, depthInt);

    GroupMemoryBarrierWithGroupSync();

    // Convert depth buffer values to linear view-space Z (positive values)
    float minDepthNDC = asfloat(TileMinDepthInt);
    float maxDepthNDC = asfloat(TileMaxDepthInt);

    // Convert NDC depth to linear Z
    // minDepthNDC = closest depth (smallest NDC value) = smallest linear Z (closest to camera)
    // maxDepthNDC = farthest depth (largest NDC value) = largest linear Z (farthest from camera)
    float tileNearZ = DepthToLinearZ(minDepthNDC);  // Closest point in tile
    float tileFarZ = DepthToLinearZ(maxDepthNDC);   // Farthest point in tile

    // Ensure near < far (they should be already since smaller NDC = closer)
    if (tileNearZ > tileFarZ) {
        float temp = tileNearZ;
        tileNearZ = tileFarZ;
        tileFarZ = temp;
    }

    // Each thread tests some lights
    uint ThreadCount = TILE_SIZE * TILE_SIZE;
    uint LightsPerThread = (LightCount + ThreadCount - 1) / ThreadCount;
    uint StartLight = GroupIndex * LightsPerThread;
    uint EndLight = min(StartLight + LightsPerThread, LightCount);

    for (uint i = StartLight; i < EndLight; i++) {
        GPULight light = Lights[i];
        bool inFrustum = false;

        if (light.Type == 0) {
            // Directional light - always affects all tiles
            inFrustum = true;
        }
        else if (light.Type == 1 || light.Type == 2) {
            // Point light or Spot light - test sphere against tile frustum
            float4 lightPosView = mul(float4(light.Position, 1.0f), View);
            float range = light.Range;

            // In view space, Z is positive in front of camera (LH coordinate system)
            float lightZ = lightPosView.z;

            // Skip lights completely behind the camera
            if (lightZ + range <= 0.0f) {
                continue;
            }

            float lightNearZ = max(lightZ - range, 0.001f);  // Clamp to near plane
            float lightFarZ = lightZ + range;

            // Add conservative bias to depth range to account for precision issues
            float depthBias = 1.0f;  // Small bias in world units
            float tileNearZBiased = max(tileNearZ - depthBias, 0.001f);
            float tileFarZBiased = tileFarZ + depthBias;

            // Check depth overlap: light sphere's Z range overlaps tile's Z range
            if (lightFarZ >= tileNearZBiased && lightNearZ <= tileFarZBiased) {
                // Check against tile frustum planes (conservative sphere test)
                // Add extra range to be conservative
                float conservativeRange = range * 1.1f;  // 10% extra for safety
                inFrustum = true;
                for (int p = 0; p < 4; p++) {
                    float dist = dot(TileFrustumPlanes[p].xyz, lightPosView.xyz);
                    if (dist < -conservativeRange) {
                        inFrustum = false;
                        break;
                    }
                }
            }
        }

        if (inFrustum) {
            uint index;
            InterlockedAdd(TileLightCount, 1, index);
            if (index < MAX_LIGHTS_PER_TILE) {
                TileLightIndices[index] = i;
            }
        }
    }

    GroupMemoryBarrierWithGroupSync();

    // First thread writes results to global buffers
    if (GroupIndex == 0) {
        uint tileIndex = GroupId.y * TileCountX + GroupId.x;
        uint lightCount = min(TileLightCount, MAX_LIGHTS_PER_TILE);

        // Allocate space in global light index list
        uint globalOffset;
        InterlockedAdd(LightIndexCounter[0], lightCount, globalOffset);

        // Write to light grid
        LightGrid[tileIndex] = uint2(globalOffset, lightCount);

        // Copy light indices to global list
        for (uint j = 0; j < lightCount; j++) {
            LightIndexList[globalOffset + j] = TileLightIndices[j];
        }
    }
}
