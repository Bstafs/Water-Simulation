#define GRID_DIMENSION 256
#define WARP_GROUP_SIZE 1024

cbuffer ParticleConstantBuffer : register(b1)
{
    int particleCount;
    float wallStiffness;
    float2 padding00;

    float deltaTime;
    float smoothingLength;
    float pressure;
    float restDensity;

    float densityCoef;
    float GradPressureCoef;
    float LapViscosityCoef;
    float gravity;

    float4 vPlanes[6];
};

cbuffer SortConstantBuffer : register(b2)
{
    unsigned int sortLevel;
    unsigned int sortAlternateMask;
    unsigned int iWidth;
    unsigned int iHeight;

    float4 padding01;
};

struct IntegrateParticle
{
    float3 position;
    float padding01;

    float3 velocity;
    float padding02;
};

struct ParticleForces
{
    float3 acceleration;
    float padding01;
};

struct ParticleDensity
{
    float3 padding01;
    float density;
};

struct GridKeyStructure
{
    uint gridKey;
    uint particleIndex;
};

struct GridBorderStructure
{
    uint gridStart;
    uint gridEnd;
};

StructuredBuffer<IntegrateParticle> IntegrateInput : register(t0); // Input // Shader Resource
RWStructuredBuffer<IntegrateParticle> IntegrateOutput : register(u0); // Output // UAV

StructuredBuffer<ParticleForces> ForcesInput : register(t1); // Input
RWStructuredBuffer<ParticleForces> ForcesOutput : register(u1); // Output

StructuredBuffer<ParticleDensity> DensityInput : register(t2); // Input
RWStructuredBuffer<ParticleDensity> DensityOutput : register(u2); // Output

StructuredBuffer<GridKeyStructure> GridInput : register(t3); // Input
RWStructuredBuffer<GridKeyStructure> GridOutput : register(u3); // Output

StructuredBuffer<GridBorderStructure> GridIndicesInput : register(t4); // Input
RWStructuredBuffer<GridBorderStructure> GridIndicesOutput : register(u4); // Output

groupshared GridKeyStructure sharedData[WARP_GROUP_SIZE];

uint GetGridKey(uint3 gridIndex)
{
    uint x = gridIndex.x;
    uint y = GRID_DIMENSION * gridIndex.y;
    uint z = (GRID_DIMENSION * GRID_DIMENSION) * gridIndex.z;

    return uint(x + y + z);
}

uint3 GetGridIndex(float3 particle_position)
{
    uint halfDimension = GRID_DIMENSION * 0.5f;
    float3 scalar = float3(1.0f / smoothingLength, 1.0f / smoothingLength, 1.0f / smoothingLength);
    uint3 gridCoords = float3(scalar * particle_position) + uint3(halfDimension, halfDimension, halfDimension);
	
    return clamp(gridCoords, 0, uint3(GRID_DIMENSION - 1, GRID_DIMENSION - 1, GRID_DIMENSION - 1));
}

//--------------------------------------------------------------------------------------
// Grid Calculation
//--------------------------------------------------------------------------------------
[numthreads(256, 1, 1)]
void BuildGridCS(uint3 Gid : SV_GroupID, uint3 dispatchThreadID : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
    const unsigned int threadId = dispatchThreadID.x; // Particle ID to operate on
    float3 position = IntegrateInput[threadId].position;

    uint3 gridIndex = GetGridIndex(position);
    uint gridKey = GetGridKey(gridIndex);

    GridOutput[threadId].gridKey = gridKey;
    GridOutput[threadId].particleIndex = gridIndex;
}

[numthreads(256, 1, 1)]
void SortGridIndices(uint3 Gid : SV_GroupID, uint3 dispatchThreadID : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
    sharedData[GI] = GridInput[dispatchThreadID.x];
    GroupMemoryBarrierWithGroupSync();

    	// Sort the shared data
	[loop]
    for (unsigned int j = sortLevel >> 1; j > 0; j >>= 1)
    {
        bool which_to_take = ((sharedData[GI & ~j].gridKey <= sharedData[GI | j].gridKey) == (bool) (sortAlternateMask & dispatchThreadID.x));

        unsigned int gridKeyResult = which_to_take ? sharedData[GI ^ j].gridKey : sharedData[GI].gridKey;
        unsigned int particleIndexResult = which_to_take ? sharedData[GI ^ j].particleIndex : sharedData[GI].particleIndex;


        GroupMemoryBarrierWithGroupSync();
        
        sharedData[GI].gridKey = gridKeyResult;
        sharedData[GI].particleIndex = particleIndexResult;

        GroupMemoryBarrierWithGroupSync();
    }
    
	// Store shared data
    GridOutput[dispatchThreadID.x] = sharedData[GI];

}

[numthreads(256, 1, 1)]
void BuildGridIndicesCS(uint3 Gid : SV_GroupID, uint3 dispatchThreadID : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
    const unsigned int threadID = dispatchThreadID.x;

    unsigned int threadIdPrev = (threadID == 0) ? particleCount : threadID; threadIdPrev--;
    unsigned int threadIDNext = (threadID + 1);
    if(threadID == particleCount)
    {
        threadIDNext = 0;
    }

    unsigned int cell = GridInput[threadID].gridKey;

    unsigned int cellPrev = GridInput[threadID].gridKey;;
    if (cell != cellPrev)
    {
        GridIndicesOutput[cell].gridStart = threadID;
    }

    unsigned int cellNext = GridInput[threadIDNext].gridKey;
    if (cell != cellNext)
    {
        GridIndicesOutput[cell].gridEnd = threadID + 1;
    }
}

[numthreads(256, 1, 1)]
void ClearGridIndicesCS(uint3 Gid : SV_GroupID, uint3 dispatchThreadID : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
    GridIndicesOutput[dispatchThreadID.x].gridStart = 0;
    GridIndicesOutput[dispatchThreadID.x].gridEnd = 0;
}

//--------------------------------------------------------------------------------------
// Density Calculation
//--------------------------------------------------------------------------------------
//W_poly6(r, h) = 315 / (64 * pi * h^9) * (h^2 - r^2)^3
//g_fDensityCoef = fParticleMass * 315.0f / (64.0f * PI * Smoothlength^9)
float CalculateDensity(float r_sq)
{
    const float smoothingLengthSquared = smoothingLength * smoothingLength;

    return densityCoef * (smoothingLengthSquared - r_sq) * (smoothingLengthSquared - r_sq) * (smoothingLengthSquared - r_sq);
}

//--------------------------------------------------------------------------------------
// Force Calculation
//--------------------------------------------------------------------------------------

// Pressure = B * ((rho / rho_0)^y  - 1)
float CalculatePressure(float density)
{
    return pressure * max(pow(density / restDensity, 3.0f) - 1.0f, 0.0f);
}

// Spiky Grad Smoothing Kernel
// W_spiky(r, h) = 15 / (pi * h^6) * (h - r)^3
// GRAD( W_spikey(r, h) ) = -45 / (pi * h^6) * (h - r)^2
// GradPressureCoef = fParticleMass * -45.0f / (PI * fSmoothlen^6)
float3 CalculateGradPressure(float r, float pPressure, float nPressure, float nDesnity, float3 velDiff)
{
    const float h = smoothingLength;

    float averagePressure = 0.5f * (nPressure + pPressure);

    return GradPressureCoef * averagePressure / nDesnity * (h - r) * (h - r) / r * (velDiff);
}

// Viscosity Smoothing Kernel
// viscosity(r, h) = 15 / (2 * pi * h^3) * (-r^3 / (2 * h^3) + r^2 / h^2 + h / (2 * r) - 1)
// LAPLACIAN( W_viscosity(r, h) ) = 45 / (pi * h^6) * (h - r)
// LapViscosityCoef = fParticleMass * fViscosity * 45.0f / (PI * smoothingLength^6)
float3 CalculateLapViscosity(float r, float3 pVelocity, float3 nVelocity, float nDesnity)
{
    const float h = smoothingLength;
    float3 velDiff = (nVelocity - pVelocity);

    return LapViscosityCoef / nDesnity * (h - r) * velDiff;
}

[numthreads(256, 1, 1)]
void CSDensityMain(uint3 Gid : SV_GroupID, uint3 dispatchThreadID : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
    const unsigned int threadID = dispatchThreadID.x;

    const float smoothingSquared = smoothingLength * smoothingLength;

    float3 particlePosition = IntegrateInput[threadID].position;

    uint3 g_xyz = GetGridIndex(particlePosition);

    float density = 0.0f;

    [loop]
    for (int y = max(g_xyz.y - 1, 0); y <= min(g_xyz.y + 1, GRID_DIMENSION - 1); y++)
    {
	    [loop]
        for (uint x = max(g_xyz.x - 1, 0); x <= min(g_xyz.x + 1, GRID_DIMENSION - 1); x++)
        {
            [loop]
            for (uint z = max(g_xyz.z - 1, 0); z <= min(g_xyz.z + 1, GRID_DIMENSION - 1); z++)
            {
                uint cellID = GetGridKey(uint3(x, y, z));
                uint gridBorderStart = GridIndicesInput[cellID].gridStart;
                uint gridBorderEnd = GridIndicesInput[cellID].gridEnd;

                [loop]
                for (unsigned int nID = gridBorderStart; nID < gridBorderEnd; nID++)
                {
                    uint particleID = GridInput[nID].particleIndex;
                    float3 nPosition = IntegrateInput[particleID].position;

                    float3 diff = nPosition - particlePosition;
                    float rSQ = dot(diff, diff);

                    if(rSQ < smoothingSquared)
                    {
                        density += CalculateDensity(rSQ);
                    }
                }

            }
        }
    }
	DensityOutput[threadID].density = density;
    DensityOutput[threadID].padding01 = float3(0.0f, 0.0f, 0.0f);
}


[numthreads(256, 1, 1)]
void CSForcesMain(uint3 Gid : SV_GroupID, uint3 dispatchThreadID : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
    const unsigned int threadID = dispatchThreadID.x;
		
    float3 particlePosition = IntegrateInput[threadID].position;
    float3 particleVelocity = IntegrateInput[threadID].velocity;
    float particleDensity = DensityInput[threadID].density;
    float particlePressure = CalculatePressure(particleDensity);

    const float smoothingSquared = smoothingLength * smoothingLength;

    float3 acceleration = float3(0.0f, 0.0f, 0.0f);

    uint3 g_xyz = GetGridIndex(particlePosition);

    [loop]
    for (uint y = max(g_xyz.y - 1, 0); y <= min(g_xyz.y + 1, GRID_DIMENSION - 1); y++)
    {
        [loop]
        for (uint x = max(g_xyz.x - 1, 0); x <= min(g_xyz.x + 1, GRID_DIMENSION - 1); x++)
        {

            [loop]
            for (uint z = max(g_xyz.z - 1, 0); z <= min(g_xyz.z + 1, GRID_DIMENSION - 1); z++)
            {
                unsigned int g_cell = GetGridIndex(uint3(x, y, z));
                uint gridBorderStart = GridIndicesInput[g_cell].gridStart;
                uint gridBorderEnd = GridIndicesInput[g_cell].gridEnd;

                [loop]
                for (unsigned int nID = gridBorderStart; nID < gridBorderEnd; nID++)
                {
                    uint npID = GridInput[nID].particleIndex;
                    float3 nPosition = IntegrateInput[npID].position;

                    float3 diff = nPosition - particlePosition;
                    float rSQ = dot(diff, diff);

                    if (rSQ < smoothingSquared && threadID != npID)
                    {
                        float3 newVelocity = IntegrateInput[npID].velocity;
                        float newDensity = DensityInput[npID].density;
                        float newPressure = CalculatePressure(newDensity);
                        float r = sqrt(rSQ);

                        // apply pressure
                        acceleration += CalculateGradPressure(r, particlePressure, newPressure, newDensity, diff);
                        acceleration += CalculateLapViscosity(r, particleVelocity, newVelocity, newDensity);
                    }
                }
            }
        }
    }

    float x = length(acceleration);

    if(x != 0)
    {
        acceleration /= float3(particleDensity, particleDensity, particleDensity);
    }
    ForcesOutput[threadID].acceleration = acceleration;
    ForcesOutput[threadID].padding01 = acceleration / particleDensity;
}


[numthreads(256, 1, 1)]
void CSMain(uint3 Gid : SV_GroupID, uint3 dispatchThreadID : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
    // Integrate Particle Forces
    const unsigned int threadID = dispatchThreadID.x;

    float3 position = IntegrateInput[threadID].position;
    float3 velocity = IntegrateInput[threadID].velocity;
    float3 acceleration = ForcesInput[threadID].acceleration;

    [unroll]
    for (unsigned int i = 0; i < 6; i++)
    {
        float dist = dot(float4(position, 1), vPlanes[i]);
        acceleration += min(dist, 0) * -wallStiffness * vPlanes[i].xyz;
    }

    acceleration.y += gravity;

	velocity += acceleration * 0.03f;
    position += velocity * 0.03f;

    IntegrateOutput[threadID].position = position;
    IntegrateOutput[threadID].velocity = velocity;
    IntegrateOutput[threadID].padding01 = 0.0f;
    IntegrateOutput[threadID].padding02 = 0.0f;
}
        