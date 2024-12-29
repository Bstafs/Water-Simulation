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

static const float targetDensity = 8.0f;
static const float stiffnessValue = 30.0f;
static const float nearStiffnessValue = 30.0f;
static const float smoothingRadius = 2.5f;

// Utility functions
int3 ComputeGridIndex(float3 position)
{
    return (int3)floor(position / cellSize);
}

uint FlattenIndex(int3 index)
{
    int resolution = gridResolution;
    index = index + resolution; // Ensure indices are positive
    index = index % resolution; // Now safely wrap the indices
    return index.z * resolution * resolution + index.y * resolution + index.x;
}

[numthreads(256, 1, 1)]
void ClearGrid(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint index = dispatchThreadId.x;
    if (index >= 200)
        return;

    if (index < gridResolution * gridResolution * gridResolution * maxParticlesPerCell)
    {
        grid[index] = 0xFFFFFFFF; // Initialize as empty
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
    if (particleIndex >= 200)
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
    if (particleIndex >= 200)
        return;

    float3 position = InputPosition[particleIndex].position;
    float density = 0.0001f;
    float nearDensity = 0.0001f;

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
                    if (neighborParticleIndex == 0xFFFFFFFF)
                        continue;

                    float3 neighborPosition = InputPosition[neighborParticleIndex].position;
                    float3 direction = neighborPosition - position;
                    float distance = length(direction);

                    // Skip particles that are too far
                    if (distance == 0.0f || distance >= smoothingRadius) // Use interaction radius here
                        continue;
                                       
                    density += DensitySmoothingKernel(distance, smoothingRadius);
                    nearDensity += NearDensitySmoothingKernel(distance, smoothingRadius);                  
                }
            }
        }
    }

    OutputPosition[particleIndex].density = density;
    OutputPosition[particleIndex].nearDensity = nearDensity;
}

float ConvertDensityToPressure(float density)
{
    float densityError = density - targetDensity;
    float pressure = densityError * stiffnessValue;
    return pressure;
}

float ConvertNearDensityToNearPressure(float nearDensity)
{
    return nearDensity * nearStiffnessValue;
}

float CalculateSharedPressure(float densityA, float densityB)
{
    float pressureA = ConvertDensityToPressure(densityA);
    float pressureB = ConvertDensityToPressure(densityB);
    return (pressureA + pressureB) / 2.0f;
}

[numthreads(256, 1, 1)]
void CalculatePressure(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint particleIndex = dispatchThreadId.x;
    if (particleIndex >= 200)
        return;

    float deltaTime = InputPosition[particleIndex].deltaTime;
    float3 position = InputPosition[particleIndex].position;
    float3 velocity = InputPosition[particleIndex].velocity;
    float density = InputPosition[particleIndex].density;
    float nearDensity = InputPosition[particleIndex].nearDensity;
    
    // Pressure values for the current particle
    float pressure = ConvertDensityToPressure(density);
    float nearPressure = ConvertNearDensityToNearPressure(nearDensity);

    float3 pressureForce = float3(0.0f, 0.0f, 0.0f);
    float3 repulsionForce = float3(0.0f, 0.0f, 0.0f);
    float3 viscousForce = float3(0.0f, 0.0f, 0.0f);
    int3 gridIndex = ComputeGridIndex(position);
    float viscosityCoefficient = 0.01f;
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
                    if (neighborParticleIndex == 0xFFFFFFFF)
                        continue;

                    // Get the neighbor's position, velocity, and other properties
                    float3 neighborPosition = InputPosition[neighborParticleIndex].position;
                    float3 neighborVelocity = InputPosition[neighborParticleIndex].velocity;
                    float3 direction = neighborPosition - position;
                    float distance = length(direction);
                    
                    float3 relativeVelocity = neighborVelocity - velocity;
                    
   
                    // Calculate pressure gradient (force) between particles
                    float3 pressureDirection = direction / (distance * distance);
                    float sharedPressure = CalculateSharedPressure(density, InputPosition[neighborParticleIndex].density);
                    float sharedNearPressure = CalculateSharedPressure(nearDensity, InputPosition[neighborParticleIndex].nearDensity);
                    
                       // Skip particles that are too far
                    if (distance == 0.0f || distance >= smoothingRadius) // Use interaction radius here
                        continue;
                    
                  
                    // Apply pressure force based on pressure difference and direction
                        pressureForce += pressureDirection * PressureSmoothingKernel(distance, smoothingRadius) * sharedPressure;

                    // Calculate repulsion force using near-density
                        repulsionForce += pressureDirection * NearDensitySmoothingKernelDerivative(distance, smoothingRadius) * sharedNearPressure;
                    
                        viscousForce += viscosityCoefficient * relativeVelocity * ViscositySmoothingKernel(distance, smoothingRadius);
                    
                }
            }
        }
    }

    // Sum up the forces from pressure and repulsion
    float3 totalForce = pressureForce + repulsionForce + viscousForce;
    
    // Apply the calculated force to update the particle's velocity (acceleration = force / density)
    float3 acceleration = totalForce / density;
    OutputPosition[particleIndex].velocity.xyz += acceleration * deltaTime;
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
    float deltaTime = InputPosition[threadID].deltaTime;
   
    inputVelocity.y += -9.81f * deltaTime;
    
    inputPosition += inputVelocity * deltaTime;
    
    OutputPosition[threadID].position = inputPosition;
    OutputPosition[threadID].velocity = inputVelocity;
}
