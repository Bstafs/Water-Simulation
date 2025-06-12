#include "FluidMaths.hlsl"

struct ParticleAttributes
{
    float3 position; // 12 bytes
    float nearDensity; // 4 bytes (16-byte aligned)

    float3 velocity; // 12 bytes
    float density; // 4 bytes (16-byte aligned)
};

cbuffer SimulationParams : register(b0)
{
    int numParticles; // Total number of particles
    float deltaTime;
    float minX;
    float minZ;
};

cbuffer BitonicParams : register(b1)
{
    uint numElements;
    uint k;
    uint j;
    uint padding;
};

RWStructuredBuffer<ParticleAttributes> Partricles : register(u0); // Output UAV

RWStructuredBuffer<uint3> GridIndices : register(u1); // Grid buffer: holds indices to particles
RWStructuredBuffer<uint> GridOffsets : register(u2); // Tracks number of particles per cell

static const float targetDensity = 10.0f;
static const float stiffnessValue = 100.0f;
static const float nearStiffnessValue = 400.0f;
static const float smoothingRadius = 2.05f;
static const uint particlesPerCell = 100;
static const int ThreadCount = 128;

static const uint hashK1 = 15823;
static const uint hashK2 = 9737333;
static const uint hashK3 = 440817757;

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

[numthreads(ThreadCount, 1, 1)]
void ClearGrid(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    GridOffsets[dispatchThreadId.x] = 0;
    GridIndices[dispatchThreadId.x] = uint3(0, 0, 0);
}

[numthreads(ThreadCount, 1, 1)]
void AddParticlesToGrid(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (dispatchThreadId.x >= numParticles)
        return;

    uint index = dispatchThreadId.x;
    int3 cell = GetCell3D(Partricles[index].position, smoothingRadius);
    uint hash = HashCell3D(cell);
    uint key = KeyFromHash(hash, numParticles);
    
        // Populate the grid buffer with particle information
    GridIndices[dispatchThreadId.x] = uint3(
            index, // Store the particle index
            hash, // Store the hash
            key // Reserved for key
        );
}

// Bitonic sort compute shader
[numthreads(ThreadCount, 1, 1)]
void BitonicSort(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint i = dispatchThreadID.x;
    if (i >= numElements)
        return;

    // Compute the partner index
    uint ixj = i ^ j;

    // Only process if i < ixj 
    if (ixj > i && ixj < numElements)
    {
        // Decide the sort direction for this bitonic block
        bool ascending = ((i & k) == 0);

        // Load the two elements. 
        uint3 elemI = GridIndices[i];
        uint3 elemJ = GridIndices[ixj];

        // Compare keys 
        if ((ascending && (elemI.z > elemJ.z)) ||
    (!ascending && (elemI.z < elemJ.z)))
        {
            // Swap the two elements
            GridIndices[i] = elemJ;
            GridIndices[ixj] = elemI;
        }
    }
}

[numthreads(ThreadCount, 1, 1)]
void BuildGridOffsets(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint i = dispatchThreadID.x;
    if (i >= numElements)
        return;

    uint currentHash = GridIndices[i].y;
    uint previousHash = (i == 0) ? 0xFFFFFFFF : GridIndices[i - 1].y;

    if (i == 0 || currentHash != previousHash)
    {
        uint cellKey = KeyFromHash(currentHash, numParticles);
        GridOffsets[cellKey] = i;
    }
}

[numthreads(ThreadCount, 1, 1)]
void CalculateDensity(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (dispatchThreadId.x >= numParticles)
        return;

    
    float3 position = Partricles[dispatchThreadId.x].position;
    float density = 0.0f;
    float nearDensity = 0.0f;
    float mass = 1.0f;

    int3 gridIndex = GetCell3D(position, smoothingRadius);
    
    for (int i = 0; i < 27; i++)
    {
        uint hash = HashCell3D(gridIndex + offsets3D[i]);
        uint key = KeyFromHash(hash, numParticles);
        int currentIndex = GridOffsets[key];
         
        if (currentIndex >= numParticles)
            continue;
                
        while (currentIndex < numParticles)
        {
            uint3 indexData = GridIndices[currentIndex];
            currentIndex++;

			// Skip if hash does not match
                    
            if (indexData[1] != hash)
                continue;
                                 
            uint neighborParticleIndex = indexData[0];
            
            float3 offset = Partricles[neighborParticleIndex].position - position;
            float sqrDst = dot(offset, offset);
                     
            if (sqrDst > smoothingRadius * smoothingRadius)
                continue;
                          
            float dst = sqrt(sqrDst);
            density += mass * DensitySmoothingKernel(dst, smoothingRadius);
            nearDensity += mass * NearDensitySmoothingKernel(dst, smoothingRadius);
        }
            
        
    }

    Partricles[dispatchThreadId.x].density = density;
    Partricles[dispatchThreadId.x].nearDensity = nearDensity;
}

float ConvertDensityToPressure(float density)
{
    float densityError = density - targetDensity;
    return max(densityError, 0.0f) * stiffnessValue;
}

float ConvertNearDensityToPressure(float nearDensity)
{
    return max(nearDensity, 0.0f) * nearStiffnessValue;
}

float CalculateSharedPressure(float pressureA, float pressureB)
{
    return (pressureA + pressureB) / 2.0f;
}

[numthreads(ThreadCount, 1, 1)]
void CalculatePressure(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (dispatchThreadId.x >= numParticles)
        return;

    float3 position = Partricles[dispatchThreadId.x].position;
    float3 velocity = Partricles[dispatchThreadId.x].velocity;
    float density = Partricles[dispatchThreadId.x].density;
    float nearDensity = Partricles[dispatchThreadId.x].nearDensity;
    
    // Pressure values for the current particle
    float pressure = ConvertDensityToPressure(density);
    float nearPressure = ConvertNearDensityToPressure(nearDensity);

    float3 pressureForce = float3(0.0f, 0.0f, 0.0f);
    float3 repulsionForce = float3(0.0f, 0.0f, 0.0f);
    float3 viscousForce = float3(0.0f, 0.0f, 0.0f);
    int3 gridIndex = GetCell3D(position, smoothingRadius);
    float viscosityCoefficient = 0.01f;
    
    for (int i = 0; i < 27; i++)
    {
        uint hash = HashCell3D(gridIndex + offsets3D[i]);
        uint key = KeyFromHash(hash, numParticles);
        uint currIndex = GridOffsets[key];
     
        if (currIndex >= numParticles)
            continue;
                
        while (currIndex < numParticles)
        {
            uint3 indexData = GridIndices[currIndex];
            currIndex++;
            
			// Skip if hash does not match
            if (indexData[1] != hash)
                continue;
            
            uint neighbourIndex = indexData[0];
            if (neighbourIndex == dispatchThreadId.x)
                continue;

            
             // Get the neighbor's position, velocity, and other properties
            float3 neighborPosition = Partricles[neighbourIndex].position;
            float3 neighborVelocity = Partricles[neighbourIndex].velocity;
         
            float3 relativeVelocity = neighborVelocity - velocity;
            
            float3 offset = neighborPosition - position;
            float sqrDst = dot(offset, offset);
                                      
            if (sqrDst > smoothingRadius * smoothingRadius)
                continue;
            
            float neighborDensity = Partricles[neighbourIndex].density;
            float neighborNearDensity = Partricles[neighbourIndex].nearDensity;
            float neighbourPressure = ConvertDensityToPressure(neighborDensity);
            float neighbourPressureNear = ConvertNearDensityToPressure(neighborNearDensity);
                             
            float sharedPressure = CalculateSharedPressure(pressure, neighbourPressure);
            float sharedNearPressure = CalculateSharedPressure(nearPressure, neighbourPressureNear);
            
            float dst = sqrt(sqrDst);
                    
                    // Stops particles getting stuck inside each other and causing velocity to go NaN.
            float3 dir = dst > 0 ? offset / dst : float3(0, 1, 0);
                  
            float poly6 = ViscositySmoothingKernel(dst, smoothingRadius);
            float kernelDerivative = PressureSmoothingKernel(dst, smoothingRadius);
            float nearKernelDerivative = NearDensitySmoothingKernelDerivative(dst, smoothingRadius);
                    
            pressureForce += dir * kernelDerivative * sharedPressure / neighborDensity;
            repulsionForce += dir * nearKernelDerivative * sharedNearPressure / neighborNearDensity;
            viscousForce += relativeVelocity * viscosityCoefficient * poly6;
        }
            
        
    }

    // Sum up the forces from pressure and repulsion
    float3 totalForce = pressureForce + repulsionForce + viscousForce;
    
    // Apply the calculated force to update the particle's velocity (acceleration = force / density)
    float3 acceleration = totalForce / density;
    
    Partricles[dispatchThreadId.x].velocity.xyz += acceleration * (1.0f / 60.0f);
}

void CollisionBox(inout float3 pos, inout float3 velocity, float minX, float maxX, float minZ, float maxZ)
{
    float minY = -30.0f;

    float maxY = 50.0f;
    
    float dampingFactor = 0.99f;
    
    if (pos.x < minX)
    {
        pos.x = minX;
        velocity.x *= -1.0f;
    }
    else if (pos.x > maxX)
    {
        pos.x = maxX;
        velocity.x *= -1.0f;
    }

    if (pos.y < minY)
    {
        pos.y = minY;
        velocity.y *= -1.0f;
    }
    else if (pos.y > maxY)
    {
        pos.y = maxY;
        velocity.y *= -1.0f;
    }

    if (pos.z < minZ)
    {
        pos.z = minZ;
        velocity.z *= -1.0f;
    }
    else if (pos.z > maxZ)
    {
        pos.z = maxZ;
        velocity.z *= -1.0f;
    }

	// Apply damping to velocity after collision
    velocity *= dampingFactor;
}

[numthreads(ThreadCount, 1, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	// Integrate Particle Forces
    if (dispatchThreadID.x >= numParticles)
        return;
             
    float3 inputPosition = Partricles[dispatchThreadID.x].position;
    float3 inputVelocity = Partricles[dispatchThreadID.x].velocity;
    
    inputVelocity.y += -9.807f * 1.0f / 60.0f;
         
    inputPosition += inputVelocity * 1.0f / 60.0f;
    
    CollisionBox(inputPosition, inputVelocity, minX, -minX, minZ, -minZ);
    
    Partricles[dispatchThreadID.x].velocity = inputVelocity;
    Partricles[dispatchThreadID.x].position = inputPosition;
}
