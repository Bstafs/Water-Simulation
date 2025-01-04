#include "FluidMaths.hlsl"

struct ParticlePosition
{
    float3 position; // 12 bytes
    float deltaTime; // 4 bytes (16-byte aligned)

    float3 velocity; // 12 bytes
    float density; // 4 bytes (16-byte aligned)

    float padding; // 12 bytes
    float nearDensity; // 4 bytes (16-byte aligned)
    float minX; 
    float maxX;
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
RWStructuredBuffer<uint3> grid : register(u1); // Grid buffer: holds indices to particles
RWStructuredBuffer<int> gridCount : register(u2); // Tracks number of particles per cell

static const float targetDensity = 8.0f;
static const float stiffnessValue = 30.0f;
static const float nearStiffnessValue = 30.0f;
static const float smoothingRadius = 2.5f;

static const int3 offsets3D[27] =
{
    int3(-1, -1, -1),
	int3(-1, -1, 0),
	int3(-1, -1, 1),
	int3(-1, 0, -1),
	int3(-1, 0, 0),
	int3(-1, 0, 1),
	int3(-1, 1, -1),
	int3(-1, 1, 0),
	int3(-1, 1, 1),
	int3(0, -1, -1),
	int3(0, -1, 0),
	int3(0, -1, 1),
	int3(0, 0, -1),
	int3(0, 0, 0),
	int3(0, 0, 1),
	int3(0, 1, -1),
	int3(0, 1, 0),
	int3(0, 1, 1),
	int3(1, -1, -1),
	int3(1, -1, 0),
	int3(1, -1, 1),
	int3(1, 0, -1),
	int3(1, 0, 0),
	int3(1, 0, 1),
	int3(1, 1, -1),
	int3(1, 1, 0),
	int3(1, 1, 1)
};


static const uint hashK1 = 15823;
static const uint hashK2 = 9737333;
static const uint hashK3 = 440817757;
// Utility functions
int3 GetCell3D(float3 position, float radius)
{
    return (int3) floor(position / radius);
}

uint HashCell3D(int3 cell)
{
    cell = (uint3) cell;
    return (cell.x * hashK1) + (cell.y * hashK2) + (cell.z * hashK3);
}

uint KeyFromHash(uint hash, uint tableSize)
{
    return hash % tableSize;
}

[numthreads(256, 1, 1)]
void ClearGrid(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (dispatchThreadId.x < gridResolution * gridResolution * gridResolution)
    {
        gridCount[dispatchThreadId.x] = 0;
    }
}

[numthreads(256, 1, 1)]
void AddParticlesToGrid(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (dispatchThreadId.x >= numParticles)
        return;
    
    gridCount[dispatchThreadId.x] = numParticles;
    uint index = dispatchThreadId.x;
    int3 cell = GetCell3D(InputPosition[index].position, smoothingRadius);
    uint hash = HashCell3D(cell);
    uint key = KeyFromHash(hash, numParticles);
       
        // Populate the grid buffer with particle information
    grid[dispatchThreadId.x] = uint3(
            index, // Store the particle index
            hash, // Store the flat grid index (or a hash key)
            key // Reserved for key
        );
}


[numthreads(256, 1, 1)]
void CalculateDensity(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (dispatchThreadId.x >= numParticles)
        return;

    float3 position = InputPosition[dispatchThreadId.x].position;
    float density = 0.0001f;
    float nearDensity = 0.0001f;
    float SqrRadius = smoothingRadius * smoothingRadius;

    int3 gridIndex = GetCell3D(position, smoothingRadius);

    for (int i = 0; i < 27; i++)
    {
        uint hash = HashCell3D(gridIndex + offsets3D[i]);
        uint key = KeyFromHash(hash, numParticles);
        int currentIndex = gridCount[key];

        while (currentIndex < numParticles)
        {
            uint3 indexData = grid[currentIndex];
            currentIndex++;
			// Exit if no longer looking at correct bin
            if (indexData[2] != key)
                break;
			// Skip if hash does not match
            if (indexData[1] != hash)
                continue;
                    
            uint neighborParticleIndex = indexData[0];

            float3 neighborPosition = InputPosition[neighborParticleIndex].position;
            float3 offsetToNeighbor = neighborPosition - position;
            float sqrDstToNeighbour = dot(offsetToNeighbor, offsetToNeighbor);

 // Skip if not within radius
            if (sqrDstToNeighbour > SqrRadius)
                continue;
            
            float dst = sqrt(sqrDstToNeighbour);

            density += DensitySmoothingKernel(dst, smoothingRadius);
            nearDensity += NearDensitySmoothingKernel(dst, smoothingRadius);
        }
            
        
    }

    OutputPosition[dispatchThreadId.x].density = density;
    OutputPosition[dispatchThreadId.x].nearDensity = nearDensity;
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
    if (dispatchThreadId.x >= numParticles)
        return;

    float deltaTime = InputPosition[dispatchThreadId.x].deltaTime;
    float3 position = InputPosition[dispatchThreadId.x].position;
    float3 velocity = InputPosition[dispatchThreadId.x].velocity;
    float density = InputPosition[dispatchThreadId.x].density;
    float nearDensity = InputPosition[dispatchThreadId.x].nearDensity;
    
    // Pressure values for the current particle
    float pressure = ConvertDensityToPressure(density);
    float nearPressure = ConvertNearDensityToNearPressure(nearDensity);

    float3 pressureForce = float3(0.0f, 0.0f, 0.0f);
    float3 repulsionForce = float3(0.0f, 0.0f, 0.0f);
    float3 viscousForce = float3(0.0f, 0.0f, 0.0f);
    int3 gridIndex = GetCell3D(position, smoothingRadius);
    float viscosityCoefficient = 0.01f;
    
    for (int i = 0; i < 27; i++)
    { 
        uint hash = HashCell3D(gridIndex + offsets3D[i]);
        uint key = KeyFromHash(hash, numParticles);
        uint currIndex = gridCount[key];
     
        while (currIndex < numParticles)
        {
            uint3 indexData = grid[currIndex];
            currIndex++;
			// Exit if no longer looking at correct bin
            if (indexData[2] != key)
                break;
			// Skip if hash does not match
            if (indexData[1] != hash)
                continue;

            uint neighbourIndex = indexData[0];
			// Skip if looking at self
            if (neighbourIndex == dispatchThreadId.x)
                continue;

                    // Get the neighbor's position, velocity, and other properties
            float3 neighborPosition = InputPosition[neighbourIndex].position;
            float3 neighborVelocity = InputPosition[neighbourIndex].velocity;
            float3 direction = neighborPosition - position;
            float distance = length(direction);
                    
                                   // Skip particles that are too far
            if (distance == 0.0f || distance >= smoothingRadius) // Use interaction radius here
                continue;
            
            float3 relativeVelocity = neighborVelocity - velocity;
                      
                    // Calculate pressure gradient (force) between particles
            float3 pressureDirection = direction / (distance * distance);
            float sharedPressure = CalculateSharedPressure(density, InputPosition[neighbourIndex].density);
            float sharedNearPressure = CalculateSharedPressure(nearDensity, InputPosition[neighbourIndex].nearDensity);
                    
                                   
                    // Apply pressure force based on pressure difference and direction
            pressureForce += pressureDirection * PressureSmoothingKernel(distance, smoothingRadius) * sharedPressure;

                    // Calculate repulsion force using near-density
            repulsionForce += pressureDirection * NearDensitySmoothingKernelDerivative(distance, smoothingRadius) * sharedNearPressure;
                    
            viscousForce += viscosityCoefficient * relativeVelocity * ViscositySmoothingKernel(distance, smoothingRadius);
                    
        }
            
        
    }

    // Sum up the forces from pressure and repulsion
    float3 totalForce = pressureForce + repulsionForce + viscousForce;
    
    // Apply the calculated force to update the particle's velocity (acceleration = force / density)
    float3 acceleration = totalForce / density;
    OutputPosition[dispatchThreadId.x].velocity.xyz += acceleration * deltaTime;
}

void CollisionBox(inout float3 pos, inout float3 velocity, float minX, float maxX)
{
    float minY = -30.0f;
    float minZ = -10.0f;

    float maxY = 50.0f;
    float maxZ = 10.0f;
    
    float dampingFactor = 0.99f;
    
    if (pos.x < minX)
    {
        pos.x = minX;
        velocity.x *= -1;
    }
    else if (pos.x > maxX)
    {
        pos.x = maxX;
        velocity.x *= -1;
    }

    if (pos.y < minY)
    {
        pos.y = minY;
        velocity.y *= -1;
    }
    else if (pos.y > maxY)
    {
        pos.y = maxY;
        velocity.y *= -1;
    }

    if (pos.z < minZ)
    {
        pos.z = minZ;
        velocity.z *= -1;
    }
    else if (pos.z > maxZ)
    {
        pos.z = maxZ;
        velocity.z *= -1;
    }

		// Apply damping to velocity after collision
    velocity.x *= dampingFactor;
    velocity.y *= dampingFactor;
    velocity.z *= dampingFactor;
}

[numthreads(256, 1, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	// Integrate Particle Forces
    if (dispatchThreadID.x >= numParticles)
        return;
             
    float3 inputPosition = InputPosition[dispatchThreadID.x].position;
    float3 inputVelocity = InputPosition[dispatchThreadID.x].velocity;
    float deltaTime = InputPosition[dispatchThreadID.x].deltaTime;
    float minX = InputPosition[dispatchThreadID.x].minX;
    float maxX = InputPosition[dispatchThreadID.x].maxX;
   
    inputVelocity.y += -9.81f * deltaTime;
    
    inputPosition += inputVelocity * deltaTime;
    
    CollisionBox(inputPosition, inputVelocity, minX, maxX);
    
    OutputPosition[dispatchThreadID.x].position = inputPosition;
    OutputPosition[dispatchThreadID.x].velocity = inputVelocity;
}
