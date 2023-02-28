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

// Input
struct ConstantParticleData
{
    float3 position;
    float pressure;

    float3 velocity;
    float density;

    float3 force;
    float padding01;

    float3 acceleration;
    float padding02;
};

// Output
struct ParticleData
{
    float3 position;
    float pressure;

    float3 velocity;
    float density;
};

StructuredBuffer<ConstantParticleData> InputParticleData : register(t0); // Input
RWStructuredBuffer<ParticleData> OutputParticleData : register(u0); // Output
                                                                    
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


float FinalDensity(uint3 dispatchThreadID)
{
    const unsigned int threadID = dispatchThreadID.x;

    const float smoothingSquared = smoothingLength * smoothingLength;

    float3 particlePosition = InputParticleData[threadID].position;

    float density = 0.0f;

    for (uint i = 0; i < particleCount; i++)
    {
        float3 nPosition = InputParticleData[threadID].position;

        float3 posDiff = nPosition - particlePosition;

        float rSquared = dot(posDiff, posDiff);

        if (rSquared < smoothingSquared)
        {
            density += CalculateDensity(rSquared);
        }
    }

    return density;
}

//--------------------------------------------------------------------------------------
// Force Calculation
//--------------------------------------------------------------------------------------

// Pressure = B * ((rho / rho_0)^y  - 1)
float CalculatePressure(float density)
{
    return pressure * max(pow(density / restDensity, 3) - 1, 0);
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

float3 CalculateForce(uint3 dispatchThreadID)
{
    const unsigned int threadID = dispatchThreadID.x;
		
    float3 particlePosition = InputParticleData[threadID].position;
    float3 particleVelocity = InputParticleData[threadID].velocity;
    float particleDensity = InputParticleData[threadID].density;
    float particlePressure = CalculatePressure(particleDensity);

    const float smoothingSquared = smoothingLength * smoothingLength;

    float3 force = float3(0.0f, 0.0f, 0.0f);

    for (uint i = 0; i < particleCount; i++)
    {
        float3 newPos = InputParticleData[threadID].position;

        float3 posDiff = newPos - particlePosition;

        float rSquared = dot(posDiff, posDiff);

        if (rSquared < smoothingSquared && threadID != i)
        {
            float3 nVelocity = InputParticleData[threadID].velocity;
            float nDensity = InputParticleData[threadID].density;
            float nPressure = CalculatePressure(nDensity);
            float r = sqrt(rSquared);

            // Apply Pressure
            force += CalculateGradPressure(r, particlePressure, nPressure, nDensity, posDiff);

            // Apply Viscosity
            force += CalculateLapViscosity(r, particleVelocity, nVelocity, nDensity);
        }
    }

    return force;
}


[numthreads(10, 1, 1)]
void CSMain(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    // Integrate Particle Forces
    const unsigned int threadID = dispatchThreadID;


    float3 position = InputParticleData[threadID].position;
    float3 velocity = InputParticleData[threadID].velocity;
    float density = InputParticleData[threadID].density;
    float3 force = InputParticleData[threadID].force;
    float3 acceleration = InputParticleData[threadID].acceleration;
    float3 pressure = InputParticleData[threadID].pressure;

    density += CalculateDensity(dispatchThreadID);
    pressure += CalculatePressure(density);
    force += CalculateForce(threadID);

    acceleration = force / density - gravity;

    velocity += deltaTime * acceleration;
    position += deltaTime * velocity;

    OutputParticleData[threadID].position = position;
    OutputParticleData[threadID].velocity = velocity;
    OutputParticleData[threadID].density = density;
    OutputParticleData[threadID].pressure = pressure;
    
    
}