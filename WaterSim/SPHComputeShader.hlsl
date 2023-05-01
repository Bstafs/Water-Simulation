#define GRID_DIMENSION 256
#define WARP_GROUP_SIZE 512

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
    float4 gridDim;
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

// converts a position to a grid cell index

unsigned int GridConstructKey(uint3 xyz)
{
    // Bit pack [----Z---][----Y---][----X---]
    //             8-bit     8-bit     8-bit
    return dot(xyz.zyx, uint3(256 * 256, 256, 1));
}

unsigned int GridGetKey(unsigned int keyvaluepair)
{
    return (keyvaluepair >> 16);
}

unsigned int GridGetValue(unsigned int keyvaluepair)
{
    return (keyvaluepair & 0xFFFF);
}

uint3 GridCalculateCell(float3 position, float4 gridDim)
{
    const float3 scaledPosition = position / gridDim.xyz;
    const uint3 scaledIndex = (uint3) scaledPosition;
    const uint3 index = clamp(scaledIndex, uint3(0, 0, 0), uint3(255, 255, 255));
    return index;
}

unsigned int GridConstructKeyValuePair(uint3 xyz, uint value)
{
    // Bit pack [----Z---][----Y---][----X---][-----VALUE------]
    //             8-bit     8-bit     8-bit        8-bit
    return dot(uint4(xyz, value), uint4(256 * 256 * 256, 256 * 256, 256, 1));
}

[numthreads(256, 1, 1)]
void BuildGridCS(uint3 Gid : SV_GroupID, uint3 dispatchThreadID : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
    const unsigned int threadId = dispatchThreadID.x;

    float3 position = IntegrateInput[threadId].position;

    uint3 gridCellXYZ = GridCalculateCell(position, gridDim);

    GridOutput[threadId].gridKey = GridConstructKeyValuePair(gridCellXYZ, threadId);
}

[numthreads(256, 1, 1)]
void ClearGridIndicesCS(uint3 Gid : SV_GroupID, uint3 dispatchThreadID : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
    GridIndicesOutput[dispatchThreadID.x].gridStart = 0;
    GridIndicesOutput[dispatchThreadID.x].gridEnd = 0;
}

// determines the start and end indices of each grid cell in the particle array
[numthreads(256, 1, 1)]
void BuildGridIndicesCS(uint3 Gid : SV_GroupID, uint3 dispatchThreadID : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
    const unsigned int G_ID = dispatchThreadID.x;
    unsigned int G_ID_PREV = (G_ID == 0) ? particleCount : G_ID;
    G_ID_PREV--;
    unsigned int G_ID_NEXT = G_ID + 1;
    if (G_ID_NEXT == particleCount)
    {
        G_ID_NEXT = 0;
    }

    unsigned int cell = GridInput[G_ID].gridKey;
    unsigned int cell_prev = GridInput[G_ID_PREV].gridKey;
    unsigned int cell_next = GridInput[G_ID_NEXT].gridKey;

    if (cell != cell_prev)
    {
		// I'm the start of a cell
        GridIndicesOutput[cell].gridStart = G_ID;
    }
    if (cell != cell_next)
    {
		// I'm the end of a cell
        GridIndicesOutput[cell].gridEnd = G_ID + 1;
    }

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

        GroupMemoryBarrierWithGroupSync();

        sharedData[GI].gridKey = gridKeyResult;

        GroupMemoryBarrierWithGroupSync();
    }

	// Store shared data
    GridOutput[dispatchThreadID.x] = sharedData[GI];

}

[numthreads(1, 1, 1)]
void TransposeMatrixCS(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
    GridKeyStructure temp = GridInput[DTid.y * iWidth + DTid.x];

    GridOutput[DTid.x * iHeight + DTid.y] = temp;
}

[numthreads(256, 1, 1)]
void RearrangeParticlesCS(uint3 Gid : SV_GroupID, uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
    const unsigned int particleID = DTid.x;
    const unsigned int gridID = GridGetValue(GridInput[particleID].gridKey);
    IntegrateOutput[particleID] = IntegrateInput[gridID];
}

//--------------------------------------------------------------------------------------
// Density Calculation
//--------------------------------------------------------------------------------------
//W_poly6(r, h) = 315 / (64 * pi * h^9) * (h^2 - r^2)^3
//g_fDensityCoef = fParticleMass * 315.0f / (64.0f * PI * Smoothlength^9)
float CalculateDensity(float r_sq)
{
    const float h_sq = smoothingLength * smoothingLength;
	// Implements this equation:
	// W_poly6(r, h) = 315 / (64 * pi * h^9) * (h^2 - r^2)^3
	// g_fDensityCoef = fParticleMass * 315.0f / (64.0f * PI * fSmoothlen^9)
    float distance = max(h_sq - r_sq, 0);
    return max(densityCoef * (distance) * (distance) * (distance), 0);
}

[numthreads(256, 1, 1)]
void CSDensityMain(uint3 Gid : SV_GroupID, uint3 dispatchThreadID : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
    const unsigned int particleID = dispatchThreadID.x;

    const float h_sq = smoothingLength * smoothingLength;
    float3 particlePosition = IntegrateInput[particleID].position;

    float density = 997.0f;

    uint3 gridXYZ = GridCalculateCell(particlePosition, gridDim.xyzw);

    [fastopt]
    for (uint Y = max(gridXYZ.y - 1, 0); Y <= min(gridXYZ.y + 1, 255); Y++) // 255 - grid max dimension
    {
        [fastopt]
        for (uint X = max(gridXYZ.x - 1, 0); X <= min(gridXYZ.x + 1, 255); X++)
        {
            [fastopt]
            for (uint Z = max(gridXYZ.z - 1, 0); Z <= min(gridXYZ.z + 1, 255); Z++)
            {
                unsigned int gridCell = GridConstructKey(uint3(X, Y, Z));
                uint2 gridStartEnd = uint2(GridIndicesInput[gridCell].gridStart, GridIndicesInput[gridCell].gridStart);

                [fastopt]
                for (unsigned int newID = gridStartEnd.x; newID < gridStartEnd.y; newID++)
                {
                    float3 newParticlePosition = IntegrateInput[newID].position;

                    float3 positionDifference = newParticlePosition - particlePosition;

                    float r_sq = dot(positionDifference, positionDifference);

                    if (r_sq < h_sq)
                    {
                        density += CalculateDensity(r_sq);
                    }
                }
            }
        }
    }

    DensityOutput[particleID].density = density;
    DensityOutput[particleID].padding01 = float3(0.0f, 0.0f, 0.0f);
}

//[numthreads(256, 1, 1)]
//void CSDensityMain(uint3 Gid : SV_GroupID, uint3 dispatchThreadID : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
//{
//    const unsigned int threadID = dispatchThreadID.x;

//    const float smoothingSquared = smoothingLength * smoothingLength;

//    float3 particlePosition = IntegrateInput[threadID].position;

//   // uint3 g_xyz = GetGridIndex(particlePosition);

//    float density = 997.0f;

//	[loop]
//    for (unsigned int nID = 0; nID < particleCount; nID++)
//    {
//    //    uint particleID = GridInput[nID].gridIndex;
//        float3 nPosition = IntegrateInput[nID].position;

//        float3 diff = nPosition - particlePosition;
//        float rSQ = dot(diff, diff);

//        if (rSQ < smoothingSquared)
//        {
//            density += CalculateDensity(rSQ);
//        }
//    }


//    DensityOutput[threadID].density = density;
//    DensityOutput[threadID].padding01 = float3(0.0f, 0.0f, 0.0f);
//}

//--------------------------------------------------------------------------------------
// Force Calculation
//--------------------------------------------------------------------------------------

// Pressure = B * ((rho / rho_0)^y  - 1)
float CalculatePressure(float density)
{
	// Implements this equation:
	// Pressure = B * ((rho / rho_0)^y  - 1)
    float pow_inner = density / restDensity;
    float max_inner = pow(pow_inner, 3.0f);
    return pressure * max(max_inner - 1.0f, 0);
}

// Spiky Grad Smoothing Kernel
// W_spiky(r, h) = 15 / (pi * h^6) * (h - r)^3
// GRAD( W_spikey(r, h) ) = -45 / (pi * h^6) * (h - r)^2
// GradPressureCoef = fParticleMass * -45.0f / (PI * fSmoothlen^6)
float3 CalculateGradPressure(float r, float pPressure, float nPressure, float nDesnity, float3 velDiff)
{
    const float h = smoothingLength;
    float avg_pressure = 0.5f * (nPressure + pPressure);
	// Implements this equation:
	// W_spkiey(r, h) = 15 / (pi * h^6) * (h - r)^3
	// GRAD( W_spikey(r, h) ) = -45 / (pi * h^6) * (h - r)^2
	// g_fGradPressureCoef = fParticleMass * -45.0f / (PI * fSmoothlen^6)
    return GradPressureCoef * avg_pressure / nDesnity * (h - r) * (h - r) / r * (velDiff);
}

// Viscosity Smoothing Kernel
// viscosity(r, h) = 15 / (2 * pi * h^3) * (-r^3 / (2 * h^3) + r^2 / h^2 + h / (2 * r) - 1)
// LAPLACIAN( W_viscosity(r, h) ) = 45 / (pi * h^6) * (h - r)
// LapViscosityCoef = fParticleMass * fViscosity * 45.0f / (PI * smoothingLength^6)
float3 CalculateLapViscosity(float r, float3 pVelocity, float3 nVelocity, float nDesnity)
{
    const float h = smoothingLength;
    float3 vel_diff = (nVelocity - pVelocity);

	// Implements this equation:
	// W_viscosity(r, h) = 15 / (2 * pi * h^3) * (-r^3 / (2 * h^3) + r^2 / h^2 + h / (2 * r) - 1)
	// LAPLACIAN( W_viscosity(r, h) ) = 45 / (pi * h^6) * (h - r)
	// g_fLapViscosityCoef = fParticleMass * fViscosity * 45.0f / (PI * fSmoothlen^6)
    return (LapViscosityCoef / nDesnity) * (h - r) * vel_diff;
}

[numthreads(256, 1, 1)]
void CSForcesMain(uint3 Gid : SV_GroupID, uint3 dispatchThreadID : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
    const unsigned int particleID = dispatchThreadID.x;

    float3 particlePosition = IntegrateInput[particleID].position;
    float3 particleVelocity = IntegrateInput[particleID].velocity;
    float particleDensity = DensityInput[particleID].density;
    float particlePressure = CalculatePressure(particleDensity);

    const float smoothingSquared = smoothingLength * smoothingLength;

    float3 acceleration = float3(10.0f, 10.0f, 10.0f);

    uint3 gridXYZ = GridCalculateCell(particlePosition, gridDim.xyzw);

    [fastopt]
    for (uint Y = max(gridXYZ.y - 1, 0); Y <= min(gridXYZ.y + 1, 255); Y++) // 255 - grid max dimension
    {
        [fastopt]
        for (uint X = max(gridXYZ.x - 1, 0); X <= min(gridXYZ.x + 1, 255); X++)
        {
            [fastopt]
            for (uint Z = max(gridXYZ.z - 1, 0); Z <= min(gridXYZ.z + 1, 255); Z++)
            {
                unsigned int gridCell = GridConstructKey(uint3(X, Y, Z));
                uint2 gridStartEnd = uint2(GridIndicesInput[gridCell].gridStart, GridIndicesInput[gridCell].gridStart);

                [fastopt]
                for (unsigned int nID = gridStartEnd.x; nID < gridStartEnd.y; nID++)
                {
                    float3 nPosition = IntegrateInput[nID].position;

                    float3 diff = nPosition - particlePosition;
                    float rSQ = dot(diff, diff);

                    if (rSQ < smoothingSquared && particleID != nID && rSQ > 0)
                    {
                        float3 newVelocity = IntegrateInput[nID].velocity;
                        float newDensity = DensityInput[nID].density;
                        float newPressure = CalculatePressure(newDensity);
                        float r = sqrt(rSQ);

                        acceleration += CalculateGradPressure(r, particlePressure, newPressure, newDensity, diff);
                        acceleration += CalculateLapViscosity(r, particleVelocity, newVelocity, newDensity);
                    }
                }
            }
        }
    }

    int x = length(acceleration);

    //if (x != 0)
    //{
    //    ForcesOutput[particleID].acceleration = acceleration / particleDensity;
    //}

    ForcesOutput[particleID].acceleration = acceleration / particleDensity;
    ForcesOutput[particleID].padding01 = 0.0f;
}

//[numthreads(256, 1, 1)]
//void CSForcesMain(uint3 Gid : SV_GroupID, uint3 dispatchThreadID : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
//{
//    const unsigned int threadID = dispatchThreadID.x;

//    float3 particlePosition = IntegrateInput[threadID].position;
//    float3 particleVelocity = IntegrateInput[threadID].velocity;
//    float particleDensity = DensityInput[threadID].density;
//    float particlePressure = CalculatePressure(particleDensity);

//    const float smoothingSquared = smoothingLength * smoothingLength;

//    float3 acceleration = float3(0.0f, 0.0f, 0.0f);

//	[loop]
//    for (unsigned int nID = 0; nID < particleCount; nID++)
//    {

//        uint npID = GridInput[nID].gridIndex;
//        float3 nPosition = IntegrateInput[nID].position;

//        float3 diff = nPosition - particlePosition;
//        float rSQ = dot(diff, diff);

//        if (rSQ < smoothingSquared && threadID != nID && rSQ > 0)
//        {
//            float3 newVelocity = IntegrateInput[nID].velocity;
//            float newDensity = DensityInput[nID].density;
//            float newPressure = CalculatePressure(newDensity);
//            float r = sqrt(rSQ);

//            acceleration += CalculateGradPressure(r, particlePressure, newPressure, newDensity, diff);
//            acceleration += CalculateLapViscosity(r, particleVelocity, newVelocity, newDensity);
//        }
//    }

//    int x = length(acceleration);

//    if (x != 0)
//    {
//        ForcesOutput[threadID].acceleration = acceleration / particleDensity;
//    }
//    else
//    {
//        ForcesOutput[threadID].acceleration = acceleration;
//    }
//    ForcesOutput[threadID].padding01 = 0.0f;
//}

[numthreads(256, 1, 1)]
void CSMain(uint3 Gid : SV_GroupID, uint3 dispatchThreadID : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID, uint GI : SV_GroupIndex)
{
	// Integrate Particle Forces
    const unsigned int threadID = dispatchThreadID.x;

    float3 position = IntegrateInput[threadID].position;
    float3 velocity = IntegrateInput[threadID].velocity;
    float3 acceleration = ForcesInput[threadID].acceleration;

	// Box 
	[unroll(6)]
    for (unsigned int i = 0; i < 6; i++)
    {
        float dist = dot(float4(position, 1.0f), vPlanes[i]);
        acceleration += min(dist, 0.0f) * -wallStiffness * vPlanes[i].xyz;
    }

	acceleration.y += gravity;

    velocity += acceleration * 0.0004f;
    position += velocity * 0.0004f;

    IntegrateOutput[threadID].position = position;
    IntegrateOutput[threadID].velocity = velocity;
    IntegrateOutput[threadID].padding01 = 0.0f;
    IntegrateOutput[threadID].padding02 = 0.0f;
}
