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

StructuredBuffer<IntegrateParticle> IntegrateInput : register(t0); // Input // Shader Resource
RWStructuredBuffer<IntegrateParticle> IntegrateOutput : register(u0); // Output // UAV

StructuredBuffer<ParticleForces> ForcesInput : register(t1); // Input
RWStructuredBuffer<ParticleForces> ForcesOutput : register(u1); // Output

StructuredBuffer<ParticleDensity> DensityInput : register(t2); // Input
RWStructuredBuffer<ParticleDensity> DensityOutput : register(u2); // Output
                                                                    
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
void CSDensityMain(uint3 dispatchThreadID : SV_DispatchThreadID)
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
void CSForcesMain(uint3 dispatchThreadID : SV_DispatchThreadID)
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
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
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