#include "FluidMaths.hlsl"

struct ParticleAttributes
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
    int numParticles; // Total number of particles
    float3 padding01;
};

cbuffer BitonicParams : register(b1)
{
    uint numElements;
    uint k;
    uint j;
    uint padding;
};

StructuredBuffer<ParticleAttributes> InputPosition : register(t0);

RWStructuredBuffer<ParticleAttributes> OutputPosition : register(u0); // Output UAV
RWStructuredBuffer<uint3> GridIndices : register(u1); // Grid buffer: holds indices to particles
RWStructuredBuffer<uint> GridOffsets : register(u2); // Tracks number of particles per cell

static const float targetDensity = 8.0f;
static const float stiffnessValue = 30.0f;
static const float smoothingRadius = 2.5f;
static const uint particlesPerCell = 15;
static const int ThreadCount = 64;

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
    cell = (uint3)cell;
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
    int3 cell = GetCell3D(InputPosition[index].position, smoothingRadius);
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
        if ((ascending && (elemI.y > elemJ.y)) ||
             (!ascending && (elemI.y < elemJ.y)))
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

    if (i == 0 || GridIndices[i].y != GridIndices[i - 1].y)
    {
        // If first element or a new hash, store index
        GridOffsets[GridIndices[i].y] = i;
    }
}

[numthreads(ThreadCount, 1, 1)]
void CalculateDensity(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    if (dispatchThreadId.x >= numParticles)
        return;

    
    float3 position = InputPosition[dispatchThreadId.x].position;
    float density = 0.0f;
    float nearDensity = 0.0f;
    float mass = 1.0f;

    int3 gridIndex = GetCell3D(position, smoothingRadius);  
    
    for (int dx = -1; dx <= 1; ++dx)
    {
        for (int dy = -1; dy <= 1; ++dy)
        {
            for (int dz = -1; dz <= 1; ++dz)
            {
                int3 neighborCell = gridIndex + int3(dx, dy, dz);
                uint hash = HashCell3D(neighborCell);
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
            
                    float3 offset = InputPosition[neighborParticleIndex].position - position;                   
                    float sqrDst = dot(offset, offset);
                     
                    if (sqrDst > smoothingRadius * smoothingRadius)
                        continue;
                          
                    float dst = sqrt(sqrDst);
                    density += mass * DensitySmoothingKernel(dst, smoothingRadius);
                    nearDensity += mass * NearDensitySmoothingKernel(dst, smoothingRadius);
                }
            }
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

float CalculateSharedPressure(float pressureA, float pressureB)
{
    return (pressureA + pressureB) / 2.0f;
}

[numthreads(ThreadCount, 1, 1)]
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
    float nearPressure = ConvertDensityToPressure(nearDensity);

    float3 pressureForce = float3(0.0f, 0.0f, 0.0f);
    float3 repulsionForce = float3(0.0f, 0.0f, 0.0f);
    float3 viscousForce = float3(0.0f, 0.0f, 0.0f);
    int3 gridIndex = GetCell3D(position, smoothingRadius);
    float viscosityCoefficient = 0.05f;
    
    for (int dx = -1; dx <= 1; ++dx)
    {
        for (int dy = -1; dy <= 1; ++dy)
        {
            for (int dz = -1; dz <= 1; ++dz)
            {
                int3 neighborCell = gridIndex + int3(dx, dy, dz);
                uint hash = HashCell3D(neighborCell);
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
                    float3 neighborPosition = InputPosition[neighbourIndex].position;
                    float3 neighborVelocity = InputPosition[neighbourIndex].velocity;
         
                    float3 relativeVelocity = neighborVelocity - velocity;
            
                    float3 offset = neighborPosition - position;
                    float sqrDst = dot(offset, offset);
                                      
                    if (sqrDst > smoothingRadius * smoothingRadius)
                        continue;
            
                    float neighborDensity = InputPosition[neighbourIndex].density;
                    float neighborNearDensity = InputPosition[neighbourIndex].nearDensity;
                    float neighbourPressure = ConvertDensityToPressure(neighborDensity);
                    float neighbourPressureNear = ConvertDensityToPressure(neighborNearDensity);
                             
                    float sharedPressure = CalculateSharedPressure(pressure, neighbourPressure);
                    float sharedNearPressure = CalculateSharedPressure(nearPressure, neighbourPressureNear);
            
                    float dst = sqrt(sqrDst);
                    float3 direction = normalize(offset);
                    
                    float poly6 = ViscositySmoothingKernel(dst, smoothingRadius);
                    float kernelDerivative = PressureSmoothingKernel(dst, smoothingRadius);
                    float nearKernelDerivative = NearDensitySmoothingKernelDerivative(dst, smoothingRadius);
                    
                    pressureForce += direction * kernelDerivative * -sharedPressure;
                    repulsionForce += direction * nearKernelDerivative * -sharedNearPressure;
                    viscousForce += relativeVelocity * poly6;
                }
            }
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
    float minZ = -15.0f;

    float maxY = 50.0f;
    float maxZ = 15.0f;
    
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
             
    float3 inputPosition = InputPosition[dispatchThreadID.x].position;
    float3 inputVelocity = InputPosition[dispatchThreadID.x].velocity;
    float deltaTime = max(InputPosition[dispatchThreadID.x].deltaTime, 0.001f);
    float minX = InputPosition[dispatchThreadID.x].minX;
    float maxX = InputPosition[dispatchThreadID.x].maxX;
   
    inputVelocity.y += -9.81f * deltaTime;
    
    inputPosition += inputVelocity * deltaTime;
    
    CollisionBox(inputPosition, inputVelocity, minX, maxX);   
    
    OutputPosition[dispatchThreadID.x].position = inputPosition;
    OutputPosition[dispatchThreadID.x].velocity = inputVelocity;
}
