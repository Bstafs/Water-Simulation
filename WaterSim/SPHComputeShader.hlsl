#define GRID_DIMENSION 256

cbuffer ParticleConstantBuffer : register(b1)
{
    int particleCount;
    float3 padding00;

    float deltaTime;
    float smoothingLength;
    float pressure;
    float restDensity;

    float densityCoef;
    float GradPressureCoef;
    float LapViscosityCoef;
    float gravity;
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

uint GetGridKey(uint3 gridIndex)
{
    uint x = gridIndex.x;
    uint y = GRID_DIMENSION * gridIndex.y;
    uint z = (GRID_DIMENSION * GRID_DIMENSION) * gridIndex.z;

    return uint(x + y + z);
}

uint3 GetGrindIndex(float3 particle_position)
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

    uint3 gridIndex = GetGrindIndex(position);
    uint gridKey = GetGridKey(gridIndex);

    GridOutput[threadId].gridKey = gridKey;
    GridOutput[threadId].particleIndex = gridIndex;
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

    float density = 0.0f;

    [loop]
    for (uint i = 0; i < particleCount; i++)
    {
        float3 nPosition = IntegrateInput[threadID].position;

        float3 posDiff = nPosition - particlePosition;

        float rSquared = dot(posDiff, posDiff);

        if (rSquared < smoothingSquared)
        {
            density += CalculateDensity(rSquared);
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

    [loop]
    for (uint i = 0; i < particleCount; i++)
    {
        float3 newPos = IntegrateInput[threadID].position;

        float3 posDiff = newPos - particlePosition;

        float rSquared = dot(posDiff, posDiff);

        if (rSquared < smoothingSquared && threadID != i)
        {
            float3 nVelocity = IntegrateInput[threadID].velocity;
            float nDensity = DensityInput[threadID].density;
            float nPressure = CalculatePressure(nDensity);
            float r = sqrt(rSquared);

            // Apply Pressure
            acceleration += CalculateGradPressure(r, particlePressure, nPressure, nDensity, posDiff);

            // Apply Viscosity
            acceleration += CalculateLapViscosity(r, particleVelocity, nVelocity, nDensity);
        }
    }

    ForcesOutput[threadID].acceleration = acceleration / particleDensity;
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

   // acceleration.y -= gravity;

    velocity += acceleration * deltaTime;
    position += velocity * deltaTime;

    IntegrateOutput[threadID].position = position;
    IntegrateOutput[threadID].velocity = velocity;
    IntegrateOutput[threadID].padding01 = 0.0f;
    IntegrateOutput[threadID].padding02 = 0.0f;
}