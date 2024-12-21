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

float interactionRadius = sqrt(3.0f) * 5;

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
void NeighborSearch(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint particleIndex = dispatchThreadId.x;
    if (particleIndex >= numParticles)
        return;

    // Current particle's position
    float3 position = particlePositions[particleIndex].position;

    // Compute the current particle's grid cell index
    int3 gridIndex = ComputeGridIndex(position);

    // Loop through the current cell and its neighbors (3x3x3 cells in total)
    for (int z = -1; z <= 1; z++)
    {
        for (int y = -1; y <= 1; y++)
        {
            for (int x = -1; x <= 1; x++)
            {
                // Neighbor cell index
                int3 neighborIndex = gridIndex + int3(x, y, z);

                // Flatten the neighbor index
                uint flatIndex = FlattenIndex(neighborIndex);

                // Fetch the number of particles in the neighbor cell
                int neighborCount = gridCount[flatIndex];

                // Loop through all particles in the neighbor cell
                for (int i = 0; i < neighborCount; i++)
                {
                    // Get the particle index from the grid
                    uint neighborParticleIndex = grid[flatIndex * maxParticlesPerCell + i];
                    
                    if (neighborParticleIndex == uint(-1))
                        continue; // Skip empty slots

                    // Access the neighbor particle's position
                    float3 neighborPosition = particlePositions[neighborParticleIndex].position;

                    // Compute the interaction between the current particle and the neighbor
                    float3 direction = neighborPosition - position;
                    float distance = length(direction);

                    if (distance > 0.0f && distance < interactionRadius)
                    {
                        // Perform desired computation (e.g., force calculation, density update)
                        // Example: accumulate force
                       // float3 force = normalize(direction) * SomeForceFunction(distance);
                        // Store or apply force as needed
                    }
                }
            }
        }
    }
}