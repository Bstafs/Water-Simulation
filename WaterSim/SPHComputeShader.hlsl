#include "FluidMaths.hlsl"

struct ParticlePosition
{
    float3 position; // 12 bytes
    float deltaTime; // 4 bytes (16-byte aligned)

    float3 velocity; // 12 bytes
    float density; // 4 bytes (16-byte aligned)

    float3 pack; // 12 bytes
    float nearDensity; // 4 bytes (16-byte aligned)
};

cbuffer SimulationParams : register(b0)
{
    float cellSize; // Size of each grid cell
    int gridResolution; // Number of cells along one axis
    int maxParticlesPerCell; // Maximum particles a cell can hold
    int numParticles; // Total number of particles
};

StructuredBuffer<ParticlePosition> InputPosition : register(t0);

RWStructuredBuffer<ParticlePosition> OutputPosition : register(u0); // Output UAV
RWStructuredBuffer<uint> grid : register(u1); // Grid buffer: holds indices to particles
RWStructuredBuffer<int> gridCount : register(u2); // Tracks number of particles per cell

float targetDensity = 8.0f;
float stiffnessValue = 30.0f;
float interactionRadius = 1.25f;
float smoothingRadius = 2.5f;

// Utility functions
int3 ComputeGridIndex(float3 position)
{
    return int3(floor(position / cellSize));
}

uint FlattenIndex(int3 index)
{
    // Wrap indices within the grid bounds
    int resolution = gridResolution;
    index = (index + resolution) % resolution; // Wrap negative indices
    return index.z * resolution * resolution + index.y * resolution + index.x;
}

[numthreads(256, 1, 1)]
void ClearGrid(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint index = dispatchThreadId.x;

    if (index < gridResolution * gridResolution * gridResolution * maxParticlesPerCell)
    {
        grid[index] = uint(-1); // Initialize as empty
    }

    if (index < gridResolution * gridResolution * gridResolution)
    {
        gridCount[index] = 0; // Reset count
    }
}

[numthreads(256, 1, 1)]
void AddParticlesToGrid(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint particleIndex = dispatchThreadId.x;
    if (particleIndex >= numParticles)
        return;

    float3 position = InputPosition[particleIndex].position;
    int3 gridIndex = ComputeGridIndex(position);
    uint flatIndex = FlattenIndex(gridIndex);

    uint particleSlot;
    InterlockedAdd(gridCount[flatIndex], 1, particleSlot); // Atomically increment count
    if (particleSlot < maxParticlesPerCell)
    {
        grid[flatIndex * maxParticlesPerCell + particleSlot] = particleIndex;
    }
}

[numthreads(256, 1, 1)]
void CalculateDensity(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint particleIndex = dispatchThreadId.x;
    if (particleIndex >= numParticles)
        return;

    float3 position = InputPosition[particleIndex].position;
    float density = 0.0f;
    float nearDensity = 0.0f;

    int3 gridIndex = ComputeGridIndex(position);

    for (int z = -1; z <= 1; z++)
    {
        for (int y = -1; y <= 1; y++)
        {
            for (int x = -1; x <= 1; x++)
            {
                int3 neighborIndex = gridIndex + int3(x, y, z);
                uint flatIndex = FlattenIndex(neighborIndex);

                int neighborCount = gridCount[flatIndex];

                for (int i = 0; i < neighborCount; i++)
                {
                    uint neighborParticleIndex = grid[flatIndex * maxParticlesPerCell + i];
                    if (neighborParticleIndex == uint(-1))
                        continue;

                    float3 neighborPosition = InputPosition[neighborParticleIndex].position;
                    float3 direction = neighborPosition - position;
                    float distance = length(direction);

                    if (distance > 0.0f && distance < interactionRadius)
                    {
                        density += DensitySmoothingKernel(distance, smoothingRadius);
                        nearDensity += NearDensitySmoothingKernel(distance, smoothingRadius);
                    }
                }
            }
        }
    }

    OutputPosition[particleIndex].density = density;
    OutputPosition[particleIndex].nearDensity = nearDensity;
}

[numthreads(256, 1, 1)]
void CalculatePressure(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint particleIndex = dispatchThreadId.x;
    if (particleIndex >= numParticles)
        return;

    float3 position = InputPosition[particleIndex].position;
    float density = 0.0f;
    float nearDensity = 0.0f;

    int3 gridIndex = ComputeGridIndex(position);

    for (int z = -1; z <= 1; z++)
    {
        for (int y = -1; y <= 1; y++)
        {
            for (int x = -1; x <= 1; x++)
            {
                int3 neighborIndex = gridIndex + int3(x, y, z);
                uint flatIndex = FlattenIndex(neighborIndex);

                int neighborCount = gridCount[flatIndex];

                for (int i = 0; i < neighborCount; i++)
                {
                    uint neighborParticleIndex = grid[flatIndex * maxParticlesPerCell + i];
                    if (neighborParticleIndex == uint(-1))
                        continue;

                    float3 neighborPosition = InputPosition[neighborParticleIndex].position;
                    float3 direction = neighborPosition - position;
                    float distance = length(direction);

                    if (distance > 0.0f && distance < interactionRadius)
                    {
                        density += DensitySmoothingKernel(distance, smoothingRadius);
                        nearDensity += NearDensitySmoothingKernel(distance, smoothingRadius);
                    }
                }
            }
        }
    }

    OutputPosition[particleIndex].density = density;
    OutputPosition[particleIndex].nearDensity = nearDensity;
}

[numthreads(256, 1, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	// Integrate Particle Forces
    const unsigned int threadID = dispatchThreadID.x;

    if (threadID >= 200)
        return;
    
    float3 inputPosition = InputPosition[threadID].position;
    float3 inputVelocity = InputPosition[threadID].velocity;
    float inputDensity = InputPosition[threadID].density;
    float deltaTime = InputPosition[threadID].deltaTime;
   
    inputVelocity.y += -9.81f * deltaTime;
    
    OutputPosition[threadID].position = inputPosition;
    OutputPosition[threadID].velocity = inputVelocity;
    OutputPosition[threadID].density = inputDensity;
}
