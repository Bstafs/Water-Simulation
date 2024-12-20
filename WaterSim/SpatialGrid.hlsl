// Define resource bindings
cbuffer SimulationParams : register(b0)
{
    float cellSize; // Size of each grid cell
    int gridResolution; // Number of cells along one axis
    int maxParticlesPerCell; // Maximum particles a cell can hold
    int numParticles; // Total number of particles
};

struct ParticlePosition
{
    float3 position;
    float deltaTime;
    
    float3 velocity;
    float density;
};


StructuredBuffer<ParticlePosition> particlePositions : register(t0); // Input: particle positions
RWStructuredBuffer<uint> grid : register(u0); // Grid buffer: holds indices to particles
RWStructuredBuffer<int> gridCount : register(u1); // Tracks number of particles per cell

// Utility functions
int3 ComputeGridIndex(float3 position)
{
    return int3(floor(position / cellSize));
}

uint FlattenIndex(int3 index)
{
    // Wrap negative indices for compatibility with the grid
    int resolution = gridResolution;
    index = (index + resolution) % resolution; // Ensure indices are within bounds
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

    float3 position = particlePositions[particleIndex].position;
    int3 gridIndex = ComputeGridIndex(position);
    uint flatIndex = FlattenIndex(gridIndex);

    uint particleSlot = gridCount[flatIndex];
    if (particleSlot < maxParticlesPerCell)
    {
        grid[flatIndex * maxParticlesPerCell + particleSlot] = particleIndex;
    }
}

[numthreads(256, 1, 1)]
void FindNeighbors(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    // Implement neighbor searching here (e.g., iterate over nearby grid cells).
    // This would typically write results to another buffer for density/pressure calculations.
}